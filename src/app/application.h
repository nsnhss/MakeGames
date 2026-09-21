#pragma once
#include "layer_stack.h"
#include "core/timer.h"
#include <SDL.h>
#include <string>
#include <cstdint>

// SDL 类型已在全局命名空间定义，避免在 mg 中前向声明导致 namespace 污染

namespace mg {

// 引擎 Application 基类
// 负责：SDL 窗口、主循环、事件分发、Layer 栈调度
// 不负责：具体渲染后端（Vulkan / ImGui），由子类通过虚函数扩展
class Application {
public:
    struct Desc {
        const char* title = "MakeGames";
        int width = 1600;
        int height = 900;
        bool maximized = true;
        bool resizable = true;
        bool high_dpi = true;
        uint32_t sdl_flags = SDL_WINDOW_VULKAN; // 可追加其他 SDL 标志
    };

    explicit Application(const Desc& desc);
    virtual ~Application();

    // 禁止拷贝
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // 主循环入口，阻塞直到 Close()
    void Run();

    // 请求关闭（下一帧退出 Run）
    void Close() { running_ = false; }

    bool IsRunning() const { return running_; }
    SDL_Window* GetWindow() const { return window_; }
    void DestroyWindow();
    float GetDeltaTime() const { return timer_.GetDeltaTime(); }
    const Timer& GetTimer() const { return timer_; }

    // Layer 管理
    void PushLayer(std::unique_ptr<Layer> layer);
    void PushOverlay(std::unique_ptr<Layer> overlay);

    // 子类扩展点 ─────────────────────────────
protected:
    // 在窗口创建后调用（子类可在此初始化 Vulkan / ImGui）
    virtual void OnInit() {}

    // 在 Run 退出前调用（子类可在此清理 Vulkan / ImGui）
    virtual void OnShutdown() {}

    // 每帧开始时调用（子类可在此做 ImGui::NewFrame 等）
    virtual void BeginFrame() {}

    // 每帧结束时调用（子类可在此做 Vulkan Present 等）
    virtual void EndFrame() {}

    // 当 Layer 栈为空时，Run() 会直接调用这两个扩展点（game.exe 用）
    virtual void OnUpdate(float dt) { (void)dt; }
    virtual void OnRender() {}

    // 事件预处理，返回 true 表示消费事件不再向下分发
    virtual bool OnProcessEvent(::SDL_Event* event) { return false; }

private:
    void PollEvents();
    void UpdateLayers();
    void RenderLayers();
    float CalcDeltaTime();

    Desc desc_;
    SDL_Window* window_ = nullptr;
    LayerStack layer_stack_;
    bool running_ = false;
    Timer timer_;
};

} // namespace mg
