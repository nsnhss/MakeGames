#include "application.h"
#include "core/log.h"

namespace mg {

// Application 基类：负责 SDL 初始化、窗口创建、主循环驱动。
// 派生类（LauncherApplication / EditorApplication）通过重写 OnInit/OnShutdown/BeginFrame/EndFrame 等虚函数注入业务逻辑。
// 注意：不要在构造函数中调用虚函数（如旧代码曾直接调用 OnInit()），C++ 基类构造期间虚函数分发会解析到基类版本。
Application::Application(const Desc& desc) : desc_(desc)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        MG_LOG_ERROR("Application::Application SDL_Init failed: " << SDL_GetError());
        return;
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");

    uint32_t flags = desc.sdl_flags;
    if (desc.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (desc.high_dpi)  flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    if (desc.maximized) flags |= SDL_WINDOW_MAXIMIZED;

    window_ = SDL_CreateWindow(
        desc.title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        desc.width, desc.height,
        flags);

    if (!window_) {
        MG_LOG_ERROR("Application::Application SDL_CreateWindow failed: " << SDL_GetError());
        return;
    }
}

Application::~Application()
{
    if (running_) {
        running_ = false;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

// 主循环。最小化时通过 SDL_WaitEventTimeout 降低 CPU 占用，恢复后正常跑帧。
// OnInit() 在 Run() 开头调用（而非构造函数），确保派生类构造完成后才执行初始化。
void Application::Run()
{
    if (!window_) {
        MG_LOG_ERROR("Application::Run no window, returning");
        return;
    }
    OnInit();
    running_ = true;
    while (running_) {
        // 直接查询窗口实际最小化状态（而非依赖 SDL_WINDOWEVENT_MINIMIZED 事件），
        // 避免某些 SDL 版本在窗口创建阶段错误投递 MINIMIZED 事件导致 minimized_ 永真。
        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
            SDL_Event e;
            if (SDL_WaitEventTimeout(&e, 100)) {
                if (e.type == SDL_QUIT) {
                    running_ = false;
                    break;
                }
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE
                    && e.window.windowID == SDL_GetWindowID(window_)) {
                    running_ = false;
                    break;
                }
            }
            continue;
        }

        PollEvents();

        timer_.Tick();

        BeginFrame();
        if (layer_stack_.Empty()) {
            OnUpdate(timer_.GetDeltaTime());
            OnRender();
        } else {
            UpdateLayers();
            RenderLayers();
        }
        EndFrame();
    }
    layer_stack_.DetachAll();
    OnShutdown();
}

void Application::PushLayer(std::unique_ptr<Layer> layer)
{
    layer_stack_.PushLayer(std::move(layer));
}

void Application::DestroyWindow() {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

void Application::PushOverlay(std::unique_ptr<Layer> overlay)
{
    layer_stack_.PushOverlay(std::move(overlay));
}

// 轮询 SDL 事件队列。事件分发顺序：
// 1. 全局退出/关闭事件
// 2. 子类预处理（OnProcessEvent）
// 3. 从顶层 Overlay 向下层 Layer 分发（Overlay 优先消费）
void Application::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // 窗口最小化/恢复事件：由具体子类（如 LauncherApplication）消费，用于触发 swapchain rebuild。

        if (event.type == SDL_QUIT) {
            running_ = false;
            return;
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
            && event.window.windowID == SDL_GetWindowID(window_)) {
            running_ = false;
            return;
        }

        // 子类预处理（如 LauncherApplication 处理 SDL_WINDOWEVENT_RESIZED 触发 swapchain rebuild）
        if (OnProcessEvent(&event))
            continue;

        // Layer 栈事件分发：从顶层（Overlay）向下层遍历，谁先消费（返回 true）谁截断。
        for (auto it = layer_stack_.rbegin(); it != layer_stack_.rend(); ++it) {
            if ((*it)->OnEvent(&event))
                break;
        }
    }
}

// 按从底到顶顺序调用各 Layer 的 OnUpdate，传入当前帧 delta time。
void Application::UpdateLayers()
{
    for (auto* layer : layer_stack_)
        layer->OnUpdate(timer_.GetDeltaTime());
}

// 按从底到顶顺序调用各 Layer 的 OnRender。
void Application::RenderLayers()
{
    for (auto* layer : layer_stack_)
        layer->OnRender();
}

} // namespace mg
