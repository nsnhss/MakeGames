#pragma once
#include "scene/scene.h"
#include "third_party/json.hpp"
#include <memory>

namespace mg {
namespace scene {

class SceneSerializer {
public:
    static nlohmann::json Serialize(Scene* scene);
    static std::unique_ptr<Scene> Deserialize(const nlohmann::json& j);

    // 序列化到文件 / 从文件反序列化
    static bool SaveToFile(Scene* scene, const std::string& path);
    static std::unique_ptr<Scene> LoadFromFile(const std::string& path);
};

} // namespace scene
} // namespace mg
