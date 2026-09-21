#include "mg_imgui_panel.h"
#include "imgui_internal.h"
#include <unordered_map>
#include <string>

namespace mg {
namespace editor {

namespace {
    struct PanelSizeState {
        ImVec2 floating_size = ImVec2(400.0f, 500.0f);
        bool was_docked = false;
    };
    std::unordered_map<std::string, PanelSizeState> g_panel_size_states;
}

bool MG_BeginPanel(const char* title, const char* icon, bool* p_open,
                   ImGuiWindowFlags extra_flags)
{
    ImGuiWindowFlags flags = extra_flags;

    // Detect previous-frame dock state and restore floating size when a panel
    // is dragged out of a dock node. ImGui keeps the docked size after undock,
    // which feels broken; we remember the last floating size and re-apply it.
    ImGuiWindow* prev_window = ImGui::FindWindowByName(title);
    bool prev_docked = prev_window ? prev_window->DockIsActive : false;

    auto& state = g_panel_size_states[title];
    if (state.was_docked && !prev_docked) {
        ImGui::SetNextWindowSize(state.floating_size, ImGuiCond_Always);
    }

    bool result = ImGui::Begin(title, p_open, flags);
    if (result) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window) {
            if (!window->DockIsActive) {
                state.floating_size = window->Size;
            }
            state.was_docked = window->DockIsActive;
        }
    }
    return result;
}

void MG_EndPanel()
{
    ImGui::End();
}

} // namespace editor
} // namespace mg
