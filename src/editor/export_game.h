#pragma once
#include <string>

namespace mg {
namespace editor {

class EditorContext;

// 导出当前项目为可独立运行的游戏包
// output_dir: 输出目录（如 "export/MyGame"）
// pack_resources: true = 资源打包为单个 data.pak（输出根只留可执行文件 + DLL + data.pak）；
//                 false = 松散文件布局（调试用）
bool ExportGame(EditorContext& ctx, const std::string& output_dir, bool pack_resources);

} // namespace editor
} // namespace mg
