#pragma once
#include "layer.h"
#include <vector>
#include <memory>

namespace mg {

// Layer 优先级栈
// - Layers:      普通业务层（如游戏逻辑、编辑器面板），按插入顺序从上到下
// - Overlays:    覆盖层（如 ImGui、调试 HUD），始终在所有 Layer 之上
class LayerStack {
public:
    LayerStack() = default;
    ~LayerStack();

    // 压入普通 Layer（插入到 Overlay 之前）
    void PushLayer(std::unique_ptr<Layer> layer);

    // 压入 Overlay（始终在最顶层）
    void PushOverlay(std::unique_ptr<Layer> overlay);

    // 弹出（按名称匹配，调用 OnDetach）
    void PopLayer(const std::string& name);
    void PopOverlay(const std::string& name);

    // 遍历（Layer 在前，Overlay 在后）
    std::vector<Layer*>::const_iterator begin() const { return ordered_.begin(); }
    std::vector<Layer*>::const_iterator end()   const { return ordered_.end(); }

    std::vector<Layer*>::const_reverse_iterator rbegin() const { return ordered_.rbegin(); }
    std::vector<Layer*>::const_reverse_iterator rend()   const { return ordered_.rend(); }

    bool Empty() const { return layers_.empty() && overlays_.empty(); }

    // 强制按反序 Detach 所有 Layer/Overlay 并清空栈
    void DetachAll();

private:
    void RebuildOrder();

    std::vector<std::unique_ptr<Layer>> layers_;    // 普通层
    std::vector<std::unique_ptr<Layer>> overlays_;  // 覆盖层
    std::vector<Layer*> ordered_;                   // 合并后的遍历顺序
};

} // namespace mg
