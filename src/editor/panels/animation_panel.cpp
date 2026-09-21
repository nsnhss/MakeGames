#include "animation_panel.h"
#include "src/editor/selection_manager.h"
#include "src/editor/scene_session.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include "scene/components/tween_animation.h"
#include "../imgui/mg_imgui_icons.h"
#include "../imgui/mg_imgui_panel.h"
#include "imgui.h"
#include <algorithm>

namespace mg {
namespace editor {

namespace {

// 关键帧保持 t 升序插入，返回插入下标
template<typename TKey>
size_t InsertKeySorted(std::vector<TKey>& keys, TKey key) {
    auto it = std::lower_bound(keys.begin(), keys.end(), key.t,
        [](const TKey& k, float t) { return k.t < t; });
    return (size_t)(keys.insert(it, std::move(key)) - keys.begin());
}

} // namespace

AnimationPanel::AnimationPanel(SelectionManager& selection, SceneSession& session)
    : selection_(selection), session_(session) {}

const char* AnimationPanel::GetIcon() const {
    return ICON_FA_PLAY;
}

void AnimationPanel::Draw() {
    if (!MG_BeginPanel(GetName(), GetIcon(), &is_open_, ImGuiWindowFlags_NoDocking)) {
        MG_EndPanel();
        return;
    }

    auto* scene = session_.GetActiveScene();
    scene::World* world = scene ? scene->GetWorld() : nullptr;
    scene::EntityID entity = (selection_.GetType() == SelectionManager::Type::Entity)
        ? selection_.GetSelectedEntity() : 0;

    if (!world || entity == 0) {
        ImGui::TextDisabled("Select an entity in the Outliner.");
        MG_EndPanel();
        return;
    }

    auto* anim = world->GetComponent<scene::TweenAnimation>(entity);
    if (!anim) {
        ImGui::TextDisabled("Entity #%u has no TweenAnimation.", entity);
        if (ImGui::Button(ICON_FA_PLUS " Add TweenAnimation")) {
            world->AddComponent<scene::TweenAnimation>(entity);
            session_.MarkSceneDirty();
        }
        MG_EndPanel();
        return;
    }

    bool dirty = false;

    // ---- 顶部控制条 ----
    if (ImGui::Button(anim->playing ? ICON_FA_PAUSE " Pause" : ICON_FA_PLAY " Preview")) {
        anim->playing = !anim->playing;
        if (anim->playing) { anim->time = 0.0f; anim->base_captured = false; }
        dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP " Stop")) {
        anim->playing = false;
        anim->time = 0.0f;
        anim->base_captured = false;
        dirty = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &anim->loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Duration", &anim->duration, 0.1f, 0.5f, "%.2f s")) {
        anim->duration = std::max(anim->duration, 0.01f);
        dirty = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    {
        static const char* kEasingNames[] = {
            "Linear", "In Quad", "Out Quad", "InOut Quad",
            "In Cubic", "Out Cubic", "InOut Cubic"
        };
        int easing_idx = (int)anim->easing;
        if (ImGui::Combo("Easing", &easing_idx, kEasingNames, (int)(sizeof(kEasingNames) / sizeof(kEasingNames[0])))) {
            anim->easing = (scene::Easing)easing_idx;
            dirty = true;
        }
    }

    // 时间 scrub（暂停时拖动查看姿态；编辑器由 EditorLayer 驱动 UpdateOne 才会应用姿态，
    // 这里 scrub 仅改 time 并临时开 playing=false 时由用户按 Preview 应用）
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("Time", &anim->time, 0.0f, anim->duration, "%.2f s")) dirty = true;

    auto* transform = world->GetComponent<scene::Transform>(entity);
    auto* sprite = world->GetComponent<scene::Sprite>(entity);

    // ---- 通用轨道绘制 ----
    // capture：在当前 time 捕获实体现值插入关键帧
    int selected_key = -1;   // 本帧选中的关键帧（轨道内下标），配合 selected_track
    int selected_track = -1;

    struct TrackUI { const char* name; int track_id; size_t count; };
    TrackUI tracks[] = {
        { "Position", 0, anim->position.size() },
        { "Rotation", 1, anim->rotation.size() },
        { "Scale",    2, anim->scale.size() },
        { "Tint",     3, anim->tint.size() },
    };

    for (auto& tr : tracks) {
        if (!ImGui::TreeNode(tr.name, "%s (%zu)", tr.name, tr.count)) continue;

        // + Key：捕获现值
        ImGui::PushID(tr.track_id);
        if (ImGui::SmallButton(ICON_FA_PLUS " Key")) {
            switch (tr.track_id) {
                case 0: if (transform) {
                    math::Vec2 base = anim->base_captured ? anim->base_position : transform->position;
                    InsertKeySorted(anim->position, {anim->time,
                        transform->position.x - base.x, transform->position.y - base.y});
                } break;
                case 1: if (transform) InsertKeySorted(anim->rotation, {anim->time, transform->rotation}); break;
                case 2: if (transform) InsertKeySorted(anim->scale, {anim->time, transform->scale.x, transform->scale.y}); break;
                case 3: if (sprite) InsertKeySorted(anim->tint, {anim->time,
                    sprite->tint.r, sprite->tint.g, sprite->tint.b, sprite->tint.a}); break;
            }
            dirty = true;
        }

        // 关键帧列表
        size_t count = tr.count;
        for (size_t i = 0; i < count; ++i) {
            ImGui::PushID((int)i);
            float kt = 0.0f;
            switch (tr.track_id) {
                case 0: kt = anim->position[i].t; break;
                case 1: kt = anim->rotation[i].t; break;
                case 2: kt = anim->scale[i].t; break;
                case 3: kt = anim->tint[i].t; break;
            }
            char key_label[48];
            snprintf(key_label, sizeof(key_label), "t=%.2fs", kt);
            bool sel = (selected_track == tr.track_id && selected_key == (int)i);
            if (ImGui::Selectable(key_label, sel)) {
                selected_track = tr.track_id;
                selected_key = (int)i;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                switch (tr.track_id) {
                    case 0: anim->position.erase(anim->position.begin() + i); break;
                    case 1: anim->rotation.erase(anim->rotation.begin() + i); break;
                    case 2: anim->scale.erase(anim->scale.begin() + i); break;
                    case 3: anim->tint.erase(anim->tint.begin() + i); break;
                }
                dirty = true;
                ImGui::PopID();
                break;
            }
            // 选中关键帧的内联编辑
            if (sel) {
                ImGui::Indent();
                switch (tr.track_id) {
                    case 0: {
                        auto& k = anim->position[i];
                        if (ImGui::InputFloat("t", &k.t, 0.05f, 0.1f)) dirty = true;
                        if (ImGui::InputFloat2("offset", &k.x)) dirty = true;
                    } break;
                    case 1: {
                        auto& k = anim->rotation[i];
                        if (ImGui::InputFloat("t", &k.t, 0.05f, 0.1f)) dirty = true;
                        if (ImGui::InputFloat("deg", &k.v)) dirty = true;
                    } break;
                    case 2: {
                        auto& k = anim->scale[i];
                        if (ImGui::InputFloat("t", &k.t, 0.05f, 0.1f)) dirty = true;
                        if (ImGui::InputFloat2("scale", &k.x)) dirty = true;
                    } break;
                    case 3: {
                        auto& k = anim->tint[i];
                        if (ImGui::InputFloat("t", &k.t, 0.05f, 0.1f)) dirty = true;
                        if (ImGui::ColorEdit4("tint", &k.r)) dirty = true;
                    } break;
                }
                ImGui::Unindent();
            }
            ImGui::PopID();
        }
        ImGui::PopID();
        ImGui::TreePop();
    }

    if (dirty) session_.MarkSceneDirty();
    MG_EndPanel();
}

} // namespace editor
} // namespace mg
