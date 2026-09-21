#include "log_panel.h"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace mg {
namespace launcher {

void LogPanel::Load() {
    if (!std::filesystem::exists(log_path_)) return;
    std::ifstream file(log_path_);
    text_.assign((std::istreambuf_iterator<char>(file)),
                 std::istreambuf_iterator<char>());
}

void LogPanel::Save() {
    auto parent = std::filesystem::path(log_path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream file(log_path_);
    file << text_;
}

void LogPanel::BeginEdit() {
    editing_ = true;
    // 预分配足够大的缓冲区，避免输入时长度不够
    size_t need = text_.size() + 65536;
    if (buffer_.size() < need) buffer_.resize(need);
    std::memcpy(buffer_.data(), text_.c_str(), text_.size() + 1);
}

void LogPanel::EndEdit(bool save) {
    if (save) {
        text_ = buffer_.data();  // 读到 \0 为止
        Save();
    }
    editing_ = false;
}

} // namespace launcher
} // namespace mg
