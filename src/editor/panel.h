#pragma once
#include <vector>
#include <memory>
#include <type_traits>
#include <string>

namespace mg {
namespace editor {

// 所有编辑器面板的基类
// 参考 UE 的 SDockTab 与 Godot 的 EditorDock 的设计思想：
// 每个面板自包含，由 PanelManager 统一生命周期管理
class IPanel {
public:
    virtual ~IPanel() = default;

    // 面板显示名称（ImGui 窗口标题用）
    virtual const char* GetName() const = 0;

    // 图标字符串（FontAwesome），可选
    virtual const char* GetIcon() const { return ""; }

    // 每帧绘制，PanelManager 在 is_open_ 为 true 时调用
    virtual void Draw() = 0;

    // 面板被打开时调用（由 PanelManager 检测状态变化触发）
    virtual void OnOpen() {}

    // 面板被关闭时调用
    virtual void OnClose() {}

    // 是否为弹窗面板（不占用 Dock 空间，不显示在 Window 菜单，默认关闭）
    virtual bool IsPopup() const { return false; }

    bool IsOpen() const { return is_open_; }
    void SetOpen(bool open) { is_open_ = open; }

public:  // PanelManager 需要直接访问
    bool is_open_ = true;
    bool was_open_ = true; // 用于检测打开/关闭事件

protected:
};

// 面板管理器
// 负责注册、批量绘制所有面板，并自动生成 Window 菜单
class PanelManager {
public:
    void RegisterPanel(std::unique_ptr<IPanel> panel);

    // 每帧调用：绘制所有处于打开状态的面板，并触发 OnOpen/OnClose
    void DrawAllPanels();

    // 在 MenuBar 中调用，自动生成所有面板的显示/隐藏菜单项
    void DrawWindowMenu();

    // 保存/加载面板显隐状态
    void SaveLayout(const std::string& path) const;
    void LoadLayout(const std::string& path);

    // 重置默认布局（通过强制重新构建 DockBuilder）
    void ResetToDefaultLayout();

    // 按名称查找面板
    IPanel* FindPanel(const char* name) const;

    // 按类型查找面板
    template<typename T>
    T* FindPanel() const {
        static_assert(std::is_base_of_v<IPanel, T>, "T must derive from IPanel");
        for (const auto& p : panels_) {
            if (auto* casted = dynamic_cast<T*>(p.get()))
                return casted;
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<IPanel>>& GetPanels() const { return panels_; }

    // 统一设置所有面板的显示/隐藏（Viewport 除外）
    void SetAllVisible(bool visible);

private:
    std::vector<std::unique_ptr<IPanel>> panels_;
};

} // namespace editor
} // namespace mg
