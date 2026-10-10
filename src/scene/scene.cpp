// scene.cpp
// Scene — 游戏场景基类实现。
// 持有 World（ECS 容器），提供 Initialize/Shutdown 生命周期管理，
// OnUpdate/OnRender 为预留入口，供子类或外部 System 驱动。
#include "scene/scene.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/tag.h"
#include "scene/components/sprite.h"
#include "scene/components/event_trigger.h"

namespace mg {
namespace scene {

Scene::Scene() = default;
Scene::~Scene() = default;

void Scene::Initialize() {
    // 幂等：仅当 world 尚未创建时创建。禁止无条件重建——
    // 若在反序列化填充实体后误调用会清空整个 world（曾导致游戏场景加载即黑屏）。
    if (!world_) {
        world_ = std::make_unique<World>();
    }
}

void Scene::Shutdown() {
    world_.reset();
}

void Scene::OnUpdate(float dt) {
    (void)dt;
    // 预留：System 更新入口，子类或外部 System 通过 World 驱动
}

void Scene::OnRender() {
    // 预留：渲染入口，子类重写实现自定义渲染管线
}

} // namespace scene
} // namespace mg
