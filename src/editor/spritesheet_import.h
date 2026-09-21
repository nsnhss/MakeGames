#pragma once
// ---------------------------------------------------------------------------
// SpriteSheet 导入向导后端：拷贝 PNG → assets/，按帧网格生成 Sprite+Animator
// 的 prefab（{project}/prefabs/<name>.prefab.json），可在 Outliner 一键实例化。
// 网格计算见 resource/spritesheet_grid.h（纯逻辑，单测覆盖）。
// ---------------------------------------------------------------------------
#include <string>

namespace mg {
namespace editor {

// 导入 spritesheet：拷贝图片到 assets/、读取尺寸、按 frame_w×frame_h 切分、
// 生成带 Sprite+Animator 的 prefab。成功返回 true 并填 out_prefab_name；
// 失败返回 false 并填 out_error。
bool ImportSpriteSheetAsPrefab(const std::string& project_path,
                               const std::string& src_image_path,
                               int frame_w, int frame_h, float fps,
                               std::string& out_prefab_name,
                               std::string& out_error);

} // namespace editor
} // namespace mg
