#include "spritesheet_import.h"
#include "prefab_manager.h"
#include "resource/spritesheet_grid.h"
#include "scene/world.h"
#include "scene/components/name.h"
#include "scene/components/sprite.h"
#include "scene/components/animator.h"
#include <filesystem>

namespace mg {
namespace editor {

bool ImportSpriteSheetAsPrefab(const std::string& project_path,
                               const std::string& src_image_path,
                               int frame_w, int frame_h, float fps,
                               std::string& out_prefab_name,
                               std::string& out_error) {
    namespace fs = std::filesystem;
    out_error.clear();
    out_prefab_name.clear();

    if (project_path.empty()) { out_error = "No project open."; return false; }

    fs::path src(src_image_path);
    if (!fs::exists(src)) { out_error = "File not found: " + src_image_path; return false; }

    // 读取尺寸并计算切分网格
    int img_w = 0, img_h = 0;
    if (!GetImageSize(src_image_path, img_w, img_h)) {
        out_error = "Failed to read image (unsupported format?): " + src_image_path;
        return false;
    }
    SpriteSheetGrid grid = ComputeSheetGrid(img_w, img_h, frame_w, frame_h);

    // 拷贝到 {project}/assets/
    std::string assets_dir = project_path + "/assets";
    std::error_code ec;
    fs::create_directories(assets_dir, ec);
    fs::path dst = fs::path(assets_dir) / src.filename();
    if (src != dst) {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) { out_error = "Copy failed: " + ec.message(); return false; }
    }

    // 构造临时实体并保存为 prefab
    scene::World world;
    scene::EntityID e = world.CreateEntity();
    world.AddComponent<scene::Name>(e).value = src.filename().stem().string();
    auto& sprite = world.AddComponent<scene::Sprite>(e);
    sprite.texture_path = "assets/" + src.filename().string();
    if (grid.frame_count > 1) {
        auto& anim = world.AddComponent<scene::Animator>(e);
        anim.columns = grid.columns;
        anim.frame_count = grid.frame_count;
        anim.fps = fps > 0.0f ? fps : 4.0f;
    }

    out_prefab_name = src.filename().stem().string();
    PrefabManager prefabs;
    if (!prefabs.SavePrefab(project_path, &world, e, out_prefab_name)) {
        out_error = "Failed to save prefab.";
        out_prefab_name.clear();
        return false;
    }
    return true;
}

} // namespace editor
} // namespace mg
