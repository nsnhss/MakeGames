#include "editor/export_game.h"
#include "editor/editor_context.h"
#include "core/log.h"
#include "resource/data_manager.h"
#include "resource/vfs.h"
#include "resource/schema/project_schema.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/world.h"
#include "scene/entity.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/components/event_trigger.h"
#include "scene/action_commands.h"
#include "third_party/json.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <set>
#include <string>
#include <optional>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// Windows SDK 会定义 PlaySound 宏，与 ActionCmd::PlaySound 冲突
#ifdef PlaySound
#undef PlaySound
#endif
#endif

namespace mg {
namespace editor {

namespace fs = std::filesystem;

namespace {

std::string GetEditorExeDirectory() {
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {};
    if (::GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        fs::path p(path);
        if (p.has_parent_path()) return p.parent_path().string();
    }
#endif
    return fs::current_path().string();
}

bool CopyFileIfExists(const fs::path& src, const fs::path& dst, bool overwrite = true) {
    if (!fs::exists(src)) return false;
    fs::create_directories(dst.parent_path());
    auto options = fs::copy_options::overwrite_existing;
    try {
        fs::copy_file(src, dst, options);
        return true;
    } catch (...) {
        return false;
    }
}

bool CopyDirectoryRecursively(const fs::path& src, const fs::path& dst) {
    if (!fs::exists(src) || !fs::is_directory(src)) return false;
    try {
        fs::create_directories(dst);
        auto options = fs::copy_options::recursive | fs::copy_options::overwrite_existing;
        for (const auto& entry : fs::directory_iterator(src)) {
            fs::copy(entry.path(), dst / entry.path().filename(), options);
        }
        return true;
    } catch (const std::exception& e) {
        MG_LOG_ERROR("ExportGame: copy directory failed: " << e.what());
        return false;
    }
}

std::optional<nlohmann::json> LoadTilesets(const std::string& project_path) {
    std::string path = project_path + "/data/tilesets.json";
    if (!fs::exists(path)) return std::nullopt;
    return DataManager::Get().LoadJsonRaw(path);
}

std::string FindTilesetImage(const nlohmann::json& tilesets, const std::string& name) {
    if (!tilesets.is_array()) return {};
    for (const auto& item : tilesets) {
        if (item.value("name", "") == name) {
            return item.value("image_path", "");
        }
    }
    return {};
}

void CollectSceneDependencies(scene::Scene* scene,
                              const nlohmann::json& tilesets,
                              std::set<std::string>& out_paths) {
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();

    std::vector<scene::EntityID> entities;
    world->GetAllEntities(entities);
    for (auto e : entities) {
        auto* sprite = world->GetComponent<scene::Sprite>(e);
        if (sprite && !sprite->texture_path.empty()) {
            out_paths.insert(sprite->texture_path);
        }

        auto* tilemap = world->GetComponent<scene::Tilemap>(e);
        if (tilemap) {
            for (const auto& ts_name : tilemap->GetTilesetNames()) {
                std::string img = FindTilesetImage(tilesets, ts_name);
                if (!img.empty()) out_paths.insert(img);
            }
        }

        auto* trigger = world->GetComponent<scene::EventTrigger>(e);
        if (trigger) {
            for (const auto& page : trigger->pages) {
                for (const auto& action : page.actions) {
                    if (action.cmd == scene::ActionCmd::PlaySound ||
                        action.cmd == scene::ActionCmd::PlayBGM ||
                        action.cmd == scene::ActionCmd::PlayBGS ||
                        action.cmd == scene::ActionCmd::PlayME) {
                        if (!action.text.empty()) out_paths.insert(action.text);
                        if (!action.param.empty()) out_paths.insert(action.param);
                    } else if (action.cmd == scene::ActionCmd::CallScript) {
                        if (!action.text.empty()) out_paths.insert(action.text);
                    }
                }
            }
        }
    }
}

} // namespace

bool ExportGame(EditorContext& ctx, const std::string& output_dir, bool pack_resources) {
    namespace fs = std::filesystem;

    if (output_dir.empty()) {
        MG_LOG_ERROR("ExportGame: output_dir is empty");
        return false;
    }

    const std::string project_path = ctx.GetProjectPath();
    const ProjectManifest* manifest = ctx.GetProjectManifest();
    if (project_path.empty() || !manifest) {
        MG_LOG_ERROR("ExportGame: no project opened");
        return false;
    }

    if (!fs::exists(project_path + "/project.json")) {
        MG_LOG_ERROR("ExportGame: project.json not found in " << project_path);
        return false;
    }

    try {
        fs::path out = fs::absolute(output_dir);
        fs::path project_root = fs::absolute(project_path);
        if (out == project_root || (out.string().find(project_root.string()) == 0 && out.string().size() > project_root.string().size() && out.string()[project_root.string().size()] == fs::path::preferred_separator)) {
            MG_LOG_ERROR("ExportGame: output dir must not be inside project path");
            return false;
        }
        fs::create_directories(out);

        // 1. 保存当前项目（确保场景/索引最新）
        ctx.SaveProject();

        // 2. 白名单复制项目内容（排除 imgui.ini / saves/ / .cache/ 等编辑器运行时产物）
        const std::vector<std::string> whitelist = {
            "project.json", "maps", "data", "assets", "prefabs", "scenes"
        };
        for (const auto& item : whitelist) {
            fs::path src = fs::path(project_path) / item;
            if (!fs::exists(src)) continue;
            if (fs::is_directory(src)) {
                if (!CopyDirectoryRecursively(src, out / item)) {
                    MG_LOG_ERROR("ExportGame: failed to copy " << item);
                    return false;
                }
            } else if (!CopyFileIfExists(src, out / item)) {
                MG_LOG_ERROR("ExportGame: failed to copy " << item);
                return false;
            }
        }

        // 3. 收集并校验资源依赖（日志输出，便于排查缺失资源）
        auto tilesets = LoadTilesets(project_path);
        std::set<std::string> dependencies;

        // 3.1 当前场景
        if (ctx.GetActiveScene()) {
            CollectSceneDependencies(ctx.GetActiveScene(), tilesets.value_or(nlohmann::json{}), dependencies);
        }

        // 3.2 所有地图
        std::string maps_dir = project_path + "/maps";
        if (fs::exists(maps_dir) && fs::is_directory(maps_dir)) {
            for (const auto& entry : fs::directory_iterator(maps_dir)) {
                if (!entry.is_regular_file()) continue;
                std::string name = entry.path().filename().string();
                if (name == "MapInfos.json") continue;
                if (entry.path().extension() != ".json") continue;
                auto raw = DataManager::Get().LoadJsonRaw(entry.path().string());
                if (!raw) continue;
                auto scene = scene::SceneSerializer::Deserialize(*raw);
                if (scene) {
                    CollectSceneDependencies(scene.get(), tilesets.value_or(nlohmann::json{}), dependencies);
                }
            }
        }

        // 3.3 校验依赖文件是否存在
        for (const auto& dep : dependencies) {
            fs::path src = fs::path(project_path) / dep;
            fs::path dst = out / dep;
            if (!fs::exists(src)) {
                MG_LOG_ERROR("ExportGame: missing dependency: " << dep);
                continue;
            }
            if (!fs::exists(dst)) {
                fs::create_directories(dst.parent_path());
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
            }
        }

        // 4. 复制运行时二进制文件
        std::string editor_dir = GetEditorExeDirectory();
        fs::path editor_dir_p = editor_dir;
        fs::path build_dir = editor_dir_p.parent_path(); // cmake-build-release/editor -> cmake-build-release

        auto try_copy = [&](const fs::path& src, const char* name) -> bool {
            if (CopyFileIfExists(src, out / name)) {
                MG_LOG_INFO("ExportGame: copied " << name);
                return true;
            }
            return false;
        };

        // game.exe：优先同目录，否则 build/src/game.exe
        if (!try_copy(editor_dir_p / "game.exe", "game.exe")) {
            try_copy(build_dir / "src" / "game.exe", "game.exe");
        }

        // SDL2.dll
        if (!try_copy(editor_dir_p / "SDL2.dll", "SDL2.dll")) {
            if (!try_copy(build_dir / "SDL2.dll", "SDL2.dll")) {
                const char* sdl2_path = std::getenv("SDL2_PATH");
                if (sdl2_path) {
                    try_copy(fs::path(sdl2_path) / "bin" / "SDL2.dll", "SDL2.dll");
                }
            }
        }

        // vulkan-1.dll
        if (!try_copy(editor_dir_p / "vulkan-1.dll", "vulkan-1.dll")) {
            const char* vk_sdk = std::getenv("VULKAN_SDK");
            if (vk_sdk) {
                try_copy(fs::path(vk_sdk) / "Bin" / "vulkan-1.dll", "vulkan-1.dll");
            }
        }

        // 复制引擎内置资源（着色器 + 字体；game.exe 的 ImGui 覆盖层需要 fa-solid-900.ttf）
        fs::path editor_shaders = editor_dir_p / "assets" / "shaders";
        if (fs::exists(editor_shaders)) {
            CopyDirectoryRecursively(editor_shaders, out / "assets" / "shaders");
        } else {
            MG_LOG_ERROR("ExportGame: engine shaders not found at " << editor_shaders.string());
        }
        fs::path editor_fonts = editor_dir_p / "assets" / "fonts";
        if (fs::exists(editor_fonts)) {
            CopyDirectoryRecursively(editor_fonts, out / "assets" / "fonts");
        } else {
            MG_LOG_ERROR("ExportGame: engine fonts not found at " << editor_fonts.string());
        }

        // 6. 可选：资源打包为 data.pak（输出根只留可执行文件 + DLL + pak）
        if (pack_resources) {
            const std::set<std::string> keep_loose = {"game.exe", "SDL2.dll", "vulkan-1.dll", "data.pak"};
            std::vector<std::string> rel_paths;
            for (const auto& entry : fs::recursive_directory_iterator(out)) {
                if (!entry.is_regular_file()) continue;
                std::string rel = fs::relative(entry.path(), out).generic_string();
                if (keep_loose.contains(rel)) continue;
                rel_paths.push_back(std::move(rel));
            }
            std::sort(rel_paths.begin(), rel_paths.end());
            if (!vfs::WritePak(out.string(), rel_paths, (out / "data.pak").string())) {
                MG_LOG_ERROR("ExportGame: failed to write data.pak");
                return false;
            }
            // 删除已入包的松散文件与项目目录（只留 exe/dll/pak）
            for (const auto& rel : rel_paths) {
                std::error_code ec;
                fs::remove(out / rel, ec);
            }
            for (const auto& item : whitelist) {
                std::error_code ec;
                fs::remove_all(out / item, ec);
            }
            MG_LOG_INFO("ExportGame: packed " << rel_paths.size() << " files into data.pak");
        }

        MG_LOG_INFO("ExportGame: success -> " << out.string());
        return true;
    } catch (const std::exception& e) {
        MG_LOG_ERROR("ExportGame: exception: " << e.what());
        return false;
    }
}

} // namespace editor
} // namespace mg
