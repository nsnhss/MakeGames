#pragma once
#include <string>

namespace mg {
namespace editor {

class EditorContext;

// 导出当前项目为可独立运行的游戏包
// output_dir: 输出目录（如 "export/MyGame"）
bool ExportGame(EditorContext& ctx, const std::string& output_dir);

} // namespace editor
} // namespace mg
