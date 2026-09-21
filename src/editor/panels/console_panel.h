#pragma once
#include "src/editor/panel.h"
#include <vector>
#include <string>

namespace mg {
namespace editor {

// 控制台/输出日志面板（Output / Output Log）
// 显示引擎日志输出，支持过滤和搜索
class ConsolePanel : public IPanel {
public:
    const char* GetName() const override { return "Console"; }
    const char* GetIcon() const override;
    void Draw() override;
    void OnOpen() override;

    void AddLog(int level, const char* text);

private:
    void SyncFromEngineLog();

    char command_buffer_[256] = {};
    char search_buffer_[128] = {};
    int filter_level_ = 0; // 0=All, 1=Info, 2=Warning, 3=Error

    // 显示缓存：从 mg::core::Log::GetHistory() 增量同步
    struct LogLine {
        int level; // 0=Info, 1=Warn, 2=Error
        std::string text;
    };
    std::vector<LogLine> log_lines_;
    size_t synced_log_count_ = 0; // 已同步的 Log 历史条数
    bool auto_scroll_ = true;
    bool scroll_to_bottom_ = false;
};

} // namespace editor
} // namespace mg
