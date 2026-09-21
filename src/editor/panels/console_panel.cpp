#include "console_panel.h"
#include "core/log.h"
#include "../imgui/mg_imgui_icons.h"
#include "../imgui/mg_imgui_panel.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include <cstring>

namespace mg {
namespace editor {

const char* ConsolePanel::GetIcon() const {
    return ICON_FA_TERMINAL;
}

void ConsolePanel::OnOpen() {
    log_lines_.clear();
    synced_log_count_ = 0;
}

void ConsolePanel::AddLog(int level, const char* text) {
    log_lines_.push_back({ level, text });
    scroll_to_bottom_ = true;
}

// 从 mg::core::Log 历史增量同步新日志到显示缓存
void ConsolePanel::SyncFromEngineLog() {
    const auto& history = mg::core::Log::GetHistory();
    // Log 历史超限裁剪后比已同步数还小：全部重建
    if (history.size() < synced_log_count_) {
        log_lines_.clear();
        synced_log_count_ = 0;
    }
    for (size_t i = synced_log_count_; i < history.size(); ++i) {
        const auto& msg = history[i];
        int level = 0;
        if (msg.level == mg::core::LogLevel::Warn) level = 1;
        else if (msg.level == mg::core::LogLevel::Error) level = 2;
        log_lines_.push_back({ level, "[" + msg.timestamp + "] " + msg.text });
    }
    if (history.size() != synced_log_count_) {
        synced_log_count_ = history.size();
        scroll_to_bottom_ = true;
    }
}

void ConsolePanel::Draw() {
    if (!MG_BeginPanel("Console", ICON_FA_TERMINAL, &is_open_)) { MG_EndPanel(); return; }

    SyncFromEngineLog();

    // 外层容器：禁止滚动，确保顶部控制栏和底部输入框始终可见
    ImGui::BeginChild("##ConsoleInner", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // 顶部控制栏
    if (ImGui::SmallButton(ICON_FA_TRASH)) {
        // 只清空本地视图；引擎 Log 历史共享给其他 sink，下次同步从当前位置继续
        log_lines_.clear();
        synced_log_count_ = mg::core::Log::GetHistory().size();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear");
    ImGui::SameLine();
    if (ImGui::Checkbox(ICON_FA_ARROW_DOWN "##AutoScroll", &auto_scroll_)) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto-scroll");
    ImGui::SameLine();

    // 级别过滤
    const char* levels[] = { "All", "Info", "Warn", "Error" };
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("##LevelFilter", &filter_level_, levels, 4);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##ConsoleSearch", ICON_FA_MAGNIFYING_GLASS "...",
        search_buffer_, sizeof(search_buffer_),
        ImGuiInputTextFlags_CallbackAlways,
        ImGui_ImplSDL2_InputTextCallback);
    ImGui_ImplSDL2_RenderTextInputIME(search_buffer_, ImGui::GetColorU32(ImGuiCol_TextDisabled));

    ImGui::Separator();

    // 日志列表（可滚动的内层区域）
    float avail_h = ImGui::GetContentRegionAvail().y;
    float cmd_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("LogScroll", ImVec2(0, avail_h - cmd_h), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& line : log_lines_) {
        if (filter_level_ == 1 && line.level != 0) continue;
        if (filter_level_ == 2 && line.level != 1) continue;
        if (filter_level_ == 3 && line.level != 2) continue;

        if (search_buffer_[0] != '\0' && line.text.find(search_buffer_) == std::string::npos)
            continue;

        ImVec4 color(1, 1, 1, 1);
        switch (line.level) {
            case 1: color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break; // Warning
            case 2: color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break; // Error
        }
        ImGui::TextColored(color, "%s", line.text.c_str());
    }

    if (auto_scroll_ && scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }

    ImGui::EndChild();

    // 命令输入框
    ImGui::Separator();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##ConsoleCmd", command_buffer_, sizeof(command_buffer_),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways,
            ImGui_ImplSDL2_InputTextCallback)) {
        if (command_buffer_[0] != '\0') {
            log_lines_.push_back({ 0, std::string("> ") + command_buffer_ });
            log_lines_.push_back({ 1, "[Warn] 未知命令（控制台命令暂未实现，仅回显）" });
            command_buffer_[0] = '\0';
            scroll_to_bottom_ = true;
        }
    }
    ImGui_ImplSDL2_RenderTextInputIME(command_buffer_, ImGui::GetColorU32(ImGuiCol_CheckMark));

    ImGui::EndChild(); // ##ConsoleInner
    MG_EndPanel();
}

} // namespace editor
} // namespace mg
