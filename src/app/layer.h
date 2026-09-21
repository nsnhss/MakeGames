#pragma once
#include <SDL.h>
#include <string>

namespace mg {

// 引擎 Layer 抽象基类
// Editor / Launcher / Game 的具体逻辑通过继承 Layer 注入 Application 主循环
class Layer {
public:
    explicit Layer(const std::string& name = "Layer") : name_(name) {}
    virtual ~Layer() = default;

    // 生命周期（在 Layer 被压入栈时调用一次）
    virtual void OnAttach() {}

    // 生命周期（在 Layer 被弹出栈时调用一次）
    virtual void OnDetach() {}

    // 每帧更新逻辑（变量更新、状态机、输入处理等）
    // dt: 距离上一帧的秒数
    virtual void OnUpdate(float dt) {}

    // 每帧渲染逻辑（ImGui 绘制、Vulkan 渲染提交等）
    virtual void OnRender() {}

    // 事件回调（窗口大小变化、键盘鼠标等，由 Application 分发）
    // 返回 true 表示事件已消费，不再向下传递
    virtual bool OnEvent(SDL_Event* event) { return false; }

    const std::string& GetName() const { return name_; }

private:
    std::string name_;
};

} // namespace mg
