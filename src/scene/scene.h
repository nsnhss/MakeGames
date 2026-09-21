#pragma once
#include "scene/world.h"
#include "core/math/color.h"
#include <string>
#include <memory>

namespace mg {
namespace scene {

// 场景/地图容器：包含一个 World（ECS 实体+组件）和地图元数据（名称、尺寸、模式等）。
// Scene 是编辑器中"当前打开的场景"的内存表示，可被 SceneSerializer 序列化为 JSON。
class Scene {
public:
    enum class Mode { TopDown, SideScroller };

    Scene();
    ~Scene();

    void Initialize();
    void Shutdown();

    World* GetWorld() { return world_.get(); }
    const World* GetWorld() const { return world_.get(); }

    // 元数据
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    void SetSize(int w, int h) { width_ = w; height_ = h; }

    int GetTileSize() const { return tile_size_; }
    void SetTileSize(int size) { tile_size_ = size; }

    Mode GetMode() const { return mode_; }
    void SetMode(Mode mode) { mode_ = mode; }

    const std::string& GetBGMPath() const { return bgm_path_; }
    void SetBGMPath(const std::string& path) { bgm_path_ = path; }

    const math::Color& GetBackgroundColor() const { return background_color_; }
    void SetBackgroundColor(const math::Color& c) { background_color_ = c; }

    // 运行时更新入口
    void OnUpdate(float dt);
    void OnRender();

private:
    std::unique_ptr<World> world_;
    std::string name_;
    int width_ = 0;
    int height_ = 0;
    int tile_size_ = 32;
    Mode mode_ = Mode::TopDown;
    std::string bgm_path_;
    math::Color background_color_ = math::Color(0.07f, 0.07f, 0.08f);
};

} // namespace scene
} // namespace mg
