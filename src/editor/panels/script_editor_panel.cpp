#include "script_editor_panel.h"
#include "src/editor/scene_session.h"
#include "script/script_engine.h"
#include "../imgui/mg_imgui_icons.h"
#include "../imgui/mg_imgui_panel.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include <fstream>
#include <sstream>

namespace mg {
namespace editor {

namespace {
    constexpr size_t kMaxBufferSize = 256 * 1024; // 256 KB
}

ScriptEditorPanel::ScriptEditorPanel(SceneSession& session, std::function<scene::EventSystem*()> event_system_getter)
    : session_(session), get_event_system_(std::move(event_system_getter)) {
    buffer_.resize(kMaxBufferSize, '\0');
}

const char* ScriptEditorPanel::GetIcon() const {
    return ICON_FA_CODE;
}

void ScriptEditorPanel::OpenFile(const std::string& path) {
    SetOpen(true);
    LoadFile(path);
    focus_text_ = true;
}

void ScriptEditorPanel::LoadFile(const std::string& path) {
    current_path_ = path;
    std::ifstream f(path);
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        if (content.size() >= kMaxBufferSize) content.resize(kMaxBufferSize - 1);
        std::copy(content.begin(), content.end(), buffer_.begin());
        buffer_[content.size()] = '\0';
    } else {
        std::fill(buffer_.begin(), buffer_.end(), '\0');
    }
    dirty_ = false;
}

void ScriptEditorPanel::SaveFile() {
    if (current_path_.empty()) return;
    std::ofstream f(current_path_, std::ios::trunc);
    if (f) {
        f.write(buffer_.data(), (std::streamsize)std::strlen(buffer_.data()));
        dirty_ = false;
    }
}

void ScriptEditorPanel::Draw() {
    if (!MG_BeginPanel(GetName(), GetIcon(), &is_open_,
                       ImGuiWindowFlags_NoDocking)) { MG_EndPanel(); return; }

    ImGui::Text("%s", current_path_.empty() ? "<no file>" : current_path_.c_str());
    if (dirty_) ImGui::SameLine(), ImGui::TextColored(ImVec4(1, 0.85f, 0.3f, 1), "*");

    float avail_h = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackAlways;
    if (ImGui::InputTextMultiline("##script_code", buffer_.data(), buffer_.size(),
                                  ImVec2(-1.0f, std::max(avail_h - 8.0f, 60.0f)), flags,
                                  ImGui_ImplSDL2_InputTextCallback)) {
        dirty_ = true;
    }
    ImGui_ImplSDL2_RenderTextInputIME(buffer_.data(), ImGui::GetColorU32(ImGuiCol_TextDisabled));

    if (focus_text_) {
        ImGui::SetKeyboardFocusHere(-1);
        focus_text_ = false;
    }

    ImGui::Spacing();
    if (ImGui::Button("Run (Ctrl+Enter)")) {
        auto* event = get_event_system_ ? get_event_system_() : nullptr;
        auto* scene = session_.GetActiveScene();
        scene::World* world = scene ? scene->GetWorld() : nullptr;
        ScriptEngine::Get().SetContext(world, event);
        ScriptEngine::Get().DoString(buffer_.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveFile();
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
