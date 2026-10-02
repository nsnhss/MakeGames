#include "panel.h"
#include "imgui.h"
#include "core/debug_log.h"
#include "imgui/mg_imgui_icons.h"
#include "third_party/json.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>

namespace mg {
namespace editor {

// 注册面板到管理器（面板所有权转移给 PanelManager）
void PanelManager::RegisterPanel(std::unique_ptr<IPanel> panel) {
    if (!panel) return;
    // 弹窗面板默认关闭，避免占用 Dock 空间
    panel->is_open_ = !panel->IsPopup();
    panel->was_open_ = panel->is_open_;
    panels_.push_back(std::move(panel));
}

// 每帧绘制所有打开状态的面板，并检测打开/关闭事件触发 OnOpen/OnClose
void PanelManager::DrawAllPanels() {
    for (auto& panel : panels_) {
        // 检测打开/关闭事件
        if (panel->is_open_ && !panel->was_open_) {
            panel->OnOpen();
        } else if (!panel->is_open_ && panel->was_open_) {
            panel->OnClose();
        }
        panel->was_open_ = panel->is_open_;

        if (panel->is_open_) {
            panel->Draw();
        }
    }
}

// 在 MenuBar 的 Window 菜单中生成所有面板的显隐复选框
// 弹窗面板不显示在这里，它们由具体交互触发
void PanelManager::DrawWindowMenu() {
    for (auto& panel : panels_) {
        if (!panel->IsPopup()) {
            ImGui::MenuItem(panel->GetName(), nullptr, &panel->is_open_);
        }
    }
}

void PanelManager::SaveLayout(const std::string& path) const {
    EDITOR_LOG("PanelManager::SaveLayout path=" << path);
    try {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& panel : panels_) {
            // 弹窗面板默认关闭，不持久化其开关状态
            if (panel->IsPopup()) continue;
            j[panel->GetName()] = panel->IsOpen();
        }
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream f(path);
        if (f) f << j.dump(4);
    } catch (...) {}
}

void PanelManager::LoadLayout(const std::string& path) {
    EDITOR_LOG("PanelManager::LoadLayout path=" << path);
    try {
        std::ifstream f(path);
        if (!f) return;
        nlohmann::json j;
        f >> j;
        if (!j.is_object()) return;
        for (auto& panel : panels_) {
            // 弹窗面板保持默认关闭，不受持久化文件影响
            if (panel->IsPopup()) continue;
            std::string name = panel->GetName();
            if (j.contains(name) && j[name].is_boolean()) {
                panel->SetOpen(j[name].get<bool>());
            }
        }
    } catch (...) {}
}

// 重置布局：只打开常驻面板，弹窗面板默认关闭
void PanelManager::ResetToDefaultLayout() {
    for (auto& panel : panels_) {
        panel->SetOpen(!panel->IsPopup());
    }
}

// 按显示名称查找面板（线性搜索，面板数量少时性能可忽略）
IPanel* PanelManager::FindPanel(const char* name) const {
    for (const auto& p : panels_) {
        if (std::strcmp(p->GetName(), name) == 0)
            return p.get();
    }
    return nullptr;
}

// 统一设置所有面板的显示/隐藏
// 只影响常驻面板（Viewport 除外）；弹窗面板由用户显式触发打开/关闭，Play/Stop 不干预
void PanelManager::SetAllVisible(bool visible) {
    for (auto& panel : panels_) {
        if (panel->IsPopup()) continue;
        if (std::strcmp(panel->GetName(), "Viewport") == 0) continue;
        panel->SetOpen(visible);
    }
}

} // namespace editor
} // namespace mg
