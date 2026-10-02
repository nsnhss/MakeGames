#include "editor_layer.h"
#include "editor_context.h"
#include "core/debug_log.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/tween_animation.h"
#include "audio/audio_manager.h"

namespace mg {
namespace editor {

EditorLayer::EditorLayer(const std::string& project_path, EditorContext& ctx)
    : Layer("EditorLayer"), ctx_(ctx), project_path_(project_path),
      runtime_(this, &ctx.GetDatabases(), ctx.GetPlayMode(), project_path,
               [&ctx]() { return std::make_pair(ctx.GetViewportWidth(), ctx.GetViewportHeight()); })
{
    runtime_.SetEventSystemChangedCallback([&ctx](scene::EventSystem* es) { ctx.SetEventSystem(es); });
}

// ISceneHost：GameRuntime 的场景加载经 SceneSession，并附带编辑器副作用
// （同步场景路径/清脏标记/更新 Play 快照到新场景）
bool EditorLayer::LoadScene(const std::string& path) {
    if (!ctx_.LoadScene(path)) return false;
    ctx_.SetScenePath(path);
    ctx_.ClearSceneDirty();
    ctx_.TakeSnapshot();
    return true;
}

void EditorLayer::OnAttach()
{
    EDITOR_LOG("EditorLayer::OnAttach enter");
    editor_app_.Initialize(ctx_);
    ctx_.SetPanelManager(&editor_app_.GetPanelManager());
    // 注意：ImGui 上下文在 EditorApplication::OnInit 中创建，发生在 Run() 之后
    // 项目加载和布局恢复推迟到 OnUpdate 第一帧，确保 ImGui 已就绪
    if (project_path_.empty()) {
        ctx_.CreateDefaultScene();
    }
    EDITOR_LOG("EditorLayer::OnAttach exit");
}

void EditorLayer::OnDetach()
{
    EDITOR_LOG("EditorLayer::OnDetach enter");
    if (!project_path_.empty()) {
        editor_app_.GetPanelManager().SaveLayout(project_path_ + "/data/editor_layout.json");
    }
    EDITOR_LOG("EditorLayer::OnDetach exit");
}

void EditorLayer::OnUpdate(float dt)
{
    static int update_count = 0;
    ++update_count;
    if (update_count <= 3) EDITOR_LOG("EditorLayer::OnUpdate #" << update_count);
    auto& ctx = ctx_;

    // 第一帧：ImGui 上下文已创建，加载项目和布局
    if (!project_loaded_ && !project_path_.empty()) {
        EDITOR_LOG("EditorLayer::OnUpdate first frame project load start");
        project_loaded_ = true;
        ctx.SetProjectPath(project_path_);
        EDITOR_LOG("EditorLayer::OnUpdate calling OpenProject...");
        if (!ctx.OpenProject(project_path_)) {
            EDITOR_LOG("EditorLayer::OnUpdate OpenProject failed, creating default scene");
            ctx.CreateDefaultScene();
        } else {
            EDITOR_LOG("EditorLayer::OnUpdate OpenProject succeeded");
        }
        EDITOR_LOG("EditorLayer::OnUpdate calling LoadLayout...");
        editor_app_.GetPanelManager().LoadLayout(project_path_ + "/data/editor_layout.json");
        EDITOR_LOG("EditorLayer::OnUpdate first frame project load end");
    }

    auto state = ctx.GetPlayState();

    // ---- PlayState 变化检测：快照 + 面板显隐 ----
    if (state != last_play_state_) {
        if (state == EditorContext::PlayState::Playing
            && last_play_state_ == EditorContext::PlayState::Edit) {
            // Edit → Playing：保存快照，隐藏非 Viewport/Console 面板，关闭 ImGui 键盘导航
            ctx.TakeSnapshot();
            ctx.SetScreenMode(EditorContext::ScreenMode::Title);
            editor_app_.GetPanelManager().SetAllVisible(false);
            // Play Mode 自动隐藏 Tile Palette（记录状态，Stop 后恢复）
            if (auto* tp = editor_app_.GetPanelManager().FindPanel("Tile Palette")) {
                tile_palette_was_open_ = tp->IsOpen();
                if (tile_palette_was_open_) tp->SetOpen(false);
            }
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            // 进入 Play Mode：EventSystem 由 GameRuntime 管理（建 ES + OnStart + 标题画面）
            runtime_.EnterPlayMode();
        }
        else if (state == EditorContext::PlayState::Edit
            && last_play_state_ != EditorContext::PlayState::Edit) {
            // Playing/Paused → Edit：恢复快照，显示面板，恢复 ImGui 键盘导航
            ctx.RestoreSnapshot();
            editor_app_.GetPanelManager().SetAllVisible(true);
            // 恢复 Play Mode 前 Tile Palette 的显示状态
            if (tile_palette_was_open_) {
                if (auto* tp = editor_app_.GetPanelManager().FindPanel("Tile Palette")) {
                    tp->SetOpen(true);
                }
                tile_palette_was_open_ = false;
            }
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            runtime_.ExitPlayMode();
        }
        last_play_state_ = state;
    }

    // ---- Play Mode 系统更新 ----
    // 移动/碰撞/相机/事件/标题与存读档请求/Escape 暂停/Fade 场景切换均已下沉 GameRuntime
    if (state != EditorContext::PlayState::Edit) {
        runtime_.Update(dt);
    } else {
        // 编辑模式：仅预览选中实体的 TweenAnimation（playing=true 时）
        auto* scene = ctx.GetActiveScene();
        if (scene && scene->GetWorld()
            && ctx.GetSelection().GetType() == SelectionManager::Type::Entity) {
            auto e = ctx.GetSelection().GetSelectedEntity();
            auto* anim = scene->GetWorld()->GetComponent<scene::TweenAnimation>(e);
            if (anim && anim->playing) {
                tween_animation_system_.UpdateOne(dt, scene->GetWorld(), e);
            }
        }
    }

    // 音频 BGM 循环缓冲补充（Edit/Play 都可能播放）
    AudioManager::Get().Update();
}

void EditorLayer::OnRender()
{
    static int render_count = 0;
    ++render_count;
    if (render_count <= 3) EDITOR_LOG("EditorLayer::OnRender #" << render_count);
    editor_app_.Draw();
}

} // namespace editor
} // namespace mg
