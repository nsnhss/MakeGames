#include "editor/viewport_state.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/camera.h"
#include "scene/components/name.h"
#include <vector>

namespace mg {
namespace editor {

namespace {

scene::Camera* GetPrimaryCameraComponent(scene::World* world, uint32_t cam_entity) {
    if (!world || cam_entity == 0 || !world->IsValid(cam_entity)) return nullptr;
    return world->GetComponent<scene::Camera>(cam_entity);
}

scene::Transform* GetPrimaryCameraTransform(scene::World* world, uint32_t cam_entity) {
    if (!world || cam_entity == 0 || !world->IsValid(cam_entity)) return nullptr;
    return world->GetComponent<scene::Transform>(cam_entity);
}

} // namespace

// 切换变换工具模式（Translate / Rotate / Scale / Tile / Entity）
void ViewportState::SetGizmoMode(Gizmo mode) {
    if (gizmo_mode_ != mode) {
        gizmo_mode_ = mode;
        if (OnGizmoModeChanged) OnGizmoModeChanged();
    }
}

void ViewportState::SetCameraPosition(scene::World* world, const math::Vec2& pos) {
    auto* t = GetPrimaryCameraTransform(world, primary_camera_entity_);
    if (t) t->position = pos;
}

math::Vec2 ViewportState::GetCameraPosition(scene::World* world) const {
    auto* t = GetPrimaryCameraTransform(world, primary_camera_entity_);
    return t ? t->position : math::Vec2(0, 0);
}

void ViewportState::SetCameraZoom(scene::World* world, float zoom) {
    auto* cam = GetPrimaryCameraComponent(world, primary_camera_entity_);
    if (cam) cam->ortho_size = zoom;
}

float ViewportState::GetCameraZoom(scene::World* world) const {
    auto* cam = GetPrimaryCameraComponent(world, primary_camera_entity_);
    return cam ? cam->ortho_size : 1.0f;
}

void ViewportState::SetRenderCamera(scene::World* world, const math::Vec2& pos, float zoom) {
    SetCameraPosition(world, pos);
    SetCameraZoom(world, zoom);
}

math::Vec2 ViewportState::GetRenderCameraPosition(scene::World* world) const {
    return GetCameraPosition(world);
}

float ViewportState::GetRenderCameraZoom(scene::World* world) const {
    return GetCameraZoom(world);
}

void ViewportState::EnsurePrimaryCamera(scene::World* world) {
    // 1. 检查缓存的 primary_camera_entity_ 是否仍然有效
    if (primary_camera_entity_ != 0) {
        if (world && world->IsValid(primary_camera_entity_))
            return;
    }
    if (!world) return;

    // 2. 从加载的场景中查找已有主相机（避免重复创建）
    std::vector<scene::EntityID> cameras;
    world->Query<scene::Camera>(cameras);
    for (auto e : cameras) {
        auto* cam = world->GetComponent<scene::Camera>(e);
        if (cam && cam->is_primary) {
            primary_camera_entity_ = e;
            return;
        }
    }

    // 3. 场景中无主相机，创建默认 Main Camera
    auto cam_entity = world->CreateEntity();
    world->AddComponent<scene::Name>(cam_entity, "Main Camera");
    world->AddComponent<scene::Transform>(cam_entity, math::Vec2(0, 0));
    auto& cam = world->AddComponent<scene::Camera>(cam_entity);
    cam.is_primary = true;
    cam.ortho_size = 240.0f;
    primary_camera_entity_ = cam_entity;
}

math::Vec2 ViewportState::GetCameraWorldCenter(scene::World* world) const {
    if (!world) return math::Vec2(0, 0);
    if (primary_camera_entity_ != 0 && world->IsValid(primary_camera_entity_)) {
        if (auto* t = world->GetComponent<scene::Transform>(primary_camera_entity_)) {
            return t->position;
        }
    }
    return math::Vec2(0, 0);
}

} // namespace editor
} // namespace mg
