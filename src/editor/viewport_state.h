#pragma once
#include <cstdint>
#include <functional>
#include "core/math/vec2.h"
#include "imgui.h" // ImTextureID

namespace mg {
namespace scene { class World; }
namespace editor {

// 视口状态：变换工具（Gizmo）、网格吸附、视图模式、主相机引用、
// 视口渲染纹理/尺寸，以及对场景主 Camera 组件的读写委托。
class ViewportState {
public:
    enum class Gizmo { Translate, Rotate, Scale, Tile, Entity };
    enum class View { Lit, Unlit, Wireframe };

    void SetGizmoMode(Gizmo mode);
    Gizmo GetGizmoMode() const { return gizmo_mode_; }

    void SetSnapToGrid(bool snap) { snap_to_grid_ = snap; }
    bool IsSnapToGrid() const { return snap_to_grid_; }

    // 注：Lit/Unlit/Wireframe 目前仅为标志位，渲染路径无分支（见 Progress.md）
    void SetViewMode(View mode) { view_mode_ = mode; }
    View GetViewMode() const { return view_mode_; }

    // Wireframe 可用性（设备 fillModeNonSolid 特性，EditorApplication 初始化后设置）
    void SetWireframeSupported(bool supported) { wireframe_supported_ = supported; }
    bool IsWireframeSupported() const { return wireframe_supported_; }

    // 主相机 Entity 管理
    void SetPrimaryCameraEntity(uint32_t entity_id) { primary_camera_entity_ = entity_id; }
    uint32_t GetPrimaryCameraEntity() const { return primary_camera_entity_; }
    void EnsurePrimaryCamera(scene::World* world); // 场景中没有主相机时自动创建

    // 相机控制（委托到场景主 Camera Component）
    void SetCameraPosition(scene::World* world, const math::Vec2& pos);
    math::Vec2 GetCameraPosition(scene::World* world) const;
    void SetCameraZoom(scene::World* world, float zoom);
    float GetCameraZoom(scene::World* world) const;
    void SetRenderCamera(scene::World* world, const math::Vec2& pos, float zoom);
    math::Vec2 GetRenderCameraPosition(scene::World* world) const;
    float GetRenderCameraZoom(scene::World* world) const;

    // 当前 Camera 视口中心的世界坐标（Create 菜单用）
    math::Vec2 GetCameraWorldCenter(scene::World* world) const;

    // Viewport 渲染纹理（由 EditorApplication 设置）
    void SetViewportTexture(ImTextureID id) { viewport_texture_ = id; }
    ImTextureID GetViewportTexture() const { return viewport_texture_; }
    void SetViewportSize(uint32_t w, uint32_t h) { vp_w_ = w; vp_h_ = h; }
    uint32_t GetViewportWidth() const { return vp_w_; }
    uint32_t GetViewportHeight() const { return vp_h_; }

    std::function<void()> OnGizmoModeChanged;

private:
    Gizmo gizmo_mode_ = Gizmo::Translate;
    View view_mode_ = View::Lit;
    bool wireframe_supported_ = true;  // 由 EditorApplication 按设备特性设置
    bool snap_to_grid_ = false;
    uint32_t primary_camera_entity_ = 0;
    ImTextureID viewport_texture_ = 0;
    uint32_t vp_w_ = 0;
    uint32_t vp_h_ = 0;
};

} // namespace editor
} // namespace mg
