#pragma once
#include <string>
#include <vector>

namespace mg {
namespace launcher {

class LogPanel {
public:
    void Load();
    void Save();

    // 编辑模式
    void BeginEdit();
    void EndEdit(bool save);
    bool IsEditing() const { return editing_; }

    // 原始内容（只读模式直接显示）
    const std::string& GetText() const { return text_; }
    void SetText(const std::string& text) { text_ = text; Save(); }

    // 编辑缓冲区（供 ImGui::InputTextMultiline 使用）
    char* GetEditBuffer() { return buffer_.data(); }
    size_t GetEditBufferSize() const { return buffer_.size(); }

private:
    std::string text_;
    std::string log_path_ = "dev_log.txt";
    bool editing_ = false;
    std::vector<char> buffer_;
};

} // namespace launcher
} // namespace mg
