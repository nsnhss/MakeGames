// event_system_actions_world.cpp — 传送/移动/动画动作（从 event_system.cpp 拆分，D1）
#include "scene/event_system.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/tilemap.h"
#include "scene/components/name.h"
#include "scene/components/animator.h"

namespace mg {
namespace scene {

void EventSystem::ExecWorldActions(EventContext& ctx, const Action& action) {
    switch (action.cmd) {
        case ActionCmd::TransferPlayer: {
            if (!action.target.empty()) {
                // 跨场景传送：设置切换信号，由 EditorLayer 处理
                pending_scene_transition_ = action.target;
                pending_spawn_x_ = action.int_value;
                pending_spawn_y_ = (int)action.float_value;
                break;
            }
            // 同场景传送：移动 Player entity 到指定 tile 坐标
            std::vector<EntityID> players;
            world_->Query<Movement>(players);
            if (!players.empty()) {
                auto* t = world_->GetComponent<Transform>(players[0]);
                if (t) {
                    // 从 Tilemap 读取 tile_size
                    float tw = 32.0f, th = 32.0f;
                    std::vector<EntityID> all;
                    world_->GetAllEntities(all);
                    for (auto e : all) {
                        auto* tm = world_->GetComponent<Tilemap>(e);
                        if (tm) { tw = (float)tm->tile_width; th = (float)tm->tile_height; break; }
                    }
                    t->position.x = (float)action.int_value * tw;
                    t->position.y = (float)action.float_value * th;
                }
            }
            break;
        }
        case ActionCmd::PlayAnimation: {
            EntityID target = kInvalidEntity;
            if (!action.text.empty()) {
                std::vector<EntityID> all;
                world_->GetAllEntities(all);
                for (auto e : all) {
                    auto* name = world_->GetComponent<Name>(e);
                    if (name && name->value == action.text) { target = e; break; }
                }
            }
            if (target == kInvalidEntity) target = ctx.entity;
            if (auto* anim = world_->GetComponent<Animator>(target)) {
                anim->playing = true;
                anim->current_frame = 0;
                anim->timer = 0.0f;
            }
            break;
        }
        case ActionCmd::MoveEntity: {
            // 查找目标实体
            EntityID target = kInvalidEntity;
            if (action.target == "@self") {
                target = ctx.entity;
            } else {
                std::vector<EntityID> all;
                world_->GetAllEntities(all);
                for (auto e : all) {
                    auto* name = world_->GetComponent<Name>(e);
                    if (name && name->value == action.target) { target = e; break; }
                }
            }
            if (target == kInvalidEntity) {
                ToastMessage toast;
                toast.text = "MoveEntity: target not found";
                toast.timer = 2.0f;
                if (auto* transform = world_->GetComponent<Transform>(ctx.entity)) {
                    toast.world_pos = transform->position;
                    toast.world_pos.y += 24.0f;
                }
                toasts_.push_back(toast);
                break;
            }

            // 解析路径 "x1,y1;x2,y2;..."
            std::vector<math::Vec2> path;
            {
                const std::string& s = action.param;
                size_t start = 0;
                while (start < s.size()) {
                    size_t sep = s.find(';', start);
                    std::string pair = (sep == std::string::npos) ? s.substr(start) : s.substr(start, sep - start);
                    size_t comma = pair.find(',');
                    if (comma != std::string::npos) {
                        try {
                            float px = std::stof(pair.substr(0, comma));
                            float py = std::stof(pair.substr(comma + 1));
                            path.emplace_back(px, py);
                        } catch (...) {}
                    }
                    if (sep == std::string::npos) break;
                    start = sep + 1;
                }
            }
            if (path.empty()) {
                ToastMessage toast;
                toast.text = "MoveEntity: empty path";
                toast.timer = 2.0f;
                if (auto* transform = world_->GetComponent<Transform>(ctx.entity)) {
                    toast.world_pos = transform->position;
                    toast.world_pos.y += 24.0f;
                }
                toasts_.push_back(toast);
                break;
            }

            ctx.move_target = target;
            ctx.move_path = std::move(path);
            ctx.move_speed = action.int_value > 0 ? (float)action.int_value : 60.0f;
            ctx.move_index = 0;
            ctx.move_pending = true;
            break;
        }
        default:
            break;
    }
}

} // namespace scene
} // namespace mg
