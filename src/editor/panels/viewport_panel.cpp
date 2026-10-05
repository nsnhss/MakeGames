// viewport_panel.cpp — 中央视口面板（主流程）。
// 编辑模式：相机导航（滚轮缩放/中键平移）、点选、底部浮动工具栏（Gizmo 模式 + ViewMode）；
//           右键菜单/Gizmo/网格/实体放置/Tile 绘制拆分至 viewport_context_menu.cpp、
//           viewport_gizmo.cpp、viewport_tile_editing.cpp（D4）。
// Play Mode：标题画面、暂停菜单（存档槽位/物品/状态）、对话框/选项、Toast、
//            Fade 遮罩、商店/战斗、HUD（覆盖层 UI 实现见 gameui/playmode_ui.*，与 game.exe 共用）。
#include "viewport_panel.h"
#include "src/editor/editor_context.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_extensions.h"
#include "src/editor/imgui/mg_imgui_panel.h"
#include "scene/scene.h"
#include "scene/constants.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/tilemap.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mg {
namespace editor {

// ---------------------------------------------------------------------------
// 坐标换算：相机为 Y-up 世界坐标，ortho_size（zoom 参数）= 半高（世界单位），
// NDC Y 翻转（见 CameraSystem::ComputeViewProj）。img_min/img_max 为 letterbox
// 后的屏幕矩形，vp_w/vp_h 为离屏纹理尺寸（仅用于宽高比）。
// ---------------------------------------------------------------------------
bool MouseToWorld(float mx, float my, ImVec2 img_min, ImVec2 img_max,
                  math::Vec2 cam_pos, float zoom, float vp_w, float vp_h,
                  float* out_wx, float* out_wy) {
    float img_w = img_max.x - img_min.x;
    float img_h = img_max.y - img_min.y;
    if (img_w <= 0.0f || img_h <= 0.0f || vp_h <= 0.0f || zoom <= 0.0f) return false;
    float u = (mx - img_min.x) / img_w;   // 0..1
    float v = (my - img_min.y) / img_h;   // 0..1（屏幕向下）
    float aspect = vp_w / vp_h;
    *out_wx = cam_pos.x + (u * 2.0f - 1.0f) * zoom * aspect;
    *out_wy = cam_pos.y - (v * 2.0f - 1.0f) * zoom;
    return true;
}

ImVec2 WorldToScreen(float wx, float wy, ImVec2 img_min, ImVec2 img_max,
                     math::Vec2 cam_pos, float zoom, float vp_w, float vp_h) {
    float aspect = vp_h > 0.0f ? vp_w / vp_h : 1.0f;
    float half_w = zoom * aspect;
    float u = half_w > 0.0f ? ((wx - cam_pos.x) / half_w + 1.0f) * 0.5f : 0.5f;
    float v = zoom > 0.0f ? (-(wy - cam_pos.y) / zoom + 1.0f) * 0.5f : 0.5f;
    return ImVec2(img_min.x + u * (img_max.x - img_min.x),
                  img_min.y + v * (img_max.y - img_min.y));
}

float PointToSegmentDist(ImVec2 p, ImVec2 a, ImVec2 b) {
    float abx = b.x - a.x, aby = b.y - a.y;
    float len2 = abx * abx + aby * aby;
    float t = len2 > 0.0f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    float dx = p.x - (a.x + abx * t);
    float dy = p.y - (a.y + aby * t);
    return std::sqrt(dx * dx + dy * dy);
}

scene::EntityID PickEntityAt(scene::World* world, float wx, float wy, float radius) {
    if (!world) return 0;
    std::vector<scene::EntityID> entities;
    world->Query<scene::Transform>(entities);
    scene::EntityID best = 0;
    float best_d = radius;
    for (auto e : entities) {
        if (world->HasComponent<scene::Tilemap>(e)) continue; // 地图背景不参与点选
        auto* t = world->GetComponent<scene::Transform>(e);
        if (!t) continue;
        float dx = t->position.x - wx;
        float dy = t->position.y - wy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < best_d) { best_d = d; best = e; }
    }
    return best;
}

const char* ViewportPanel::GetIcon() const {
    return ICON_FA_IMAGE;
}

// ---------------------------------------------------------------------------
// 主绘制
// ---------------------------------------------------------------------------
void ViewportPanel::Draw() {
    if (!MG_BeginPanel("Viewport", GetIcon(), &is_open_)) { MG_EndPanel(); return; }

    auto& ctx = ctx_;
    using PlayState = EditorContext::PlayState;
    using GizmoMode = EditorContext::GizmoMode;
    const PlayState state = ctx.GetPlayState();

    auto* scene = ctx.GetActiveScene();
    auto* world = scene ? scene->GetWorld() : nullptr;

    // ---- 1. 视口图像（letterbox / pillarbox 居中，保持宽高比）----
    ImVec2 avail = ImGui::GetContentRegionAvail();
    uint32_t vp_w = ctx.GetViewportWidth();
    uint32_t vp_h = ctx.GetViewportHeight();
    ImTextureID tex = ctx.GetViewportTexture();
    if (!tex || vp_w == 0 || vp_h == 0 || avail.x < 8.0f || avail.y < 8.0f) {
        ImGui::TextUnformatted("Viewport initializing...");
        MG_EndPanel();
        return;
    }
    float aspect = (float)vp_w / (float)vp_h;
    float img_w = avail.x;
    float img_h = img_w / aspect;
    if (img_h > avail.y) { img_h = avail.y; img_w = img_h * aspect; }
    ImVec2 origin = ImGui::GetCursorScreenPos();

    ViewportFrameCtx f;
    f.world = world;
    f.img_min = ImVec2(origin.x + (avail.x - img_w) * 0.5f,
                       origin.y + (avail.y - img_h) * 0.5f);
    f.img_max = ImVec2(f.img_min.x + img_w, f.img_min.y + img_h);

    ImGui::SetCursorScreenPos(f.img_min);
    ImGui::Image(tex, ImVec2(img_w, img_h));
    f.hovered = ImGui::IsItemHovered();

    // 渲染相机（Play Mode 由 CameraFollow 驱动，Edit 为编辑相机）
    f.cam_pos = ctx.GetRenderCameraPosition();
    f.zoom = ctx.GetRenderCameraZoom();   // = ortho_size（半高，世界单位）
    if (f.zoom <= 0.0f) f.zoom = 1.0f;
    f.aspect = aspect;
    f.half_w = f.zoom * aspect;
    f.vp_w = vp_w;
    f.vp_h = vp_h;

    f.dl = ImGui::GetWindowDrawList();
    f.mouse = ImGui::GetMousePos();
    f.mouse_valid = MouseToWorld(f.mouse.x, f.mouse.y, f.img_min, f.img_max,
                                 f.cam_pos, f.zoom, (float)vp_w, (float)vp_h,
                                 &f.mouse_wx, &f.mouse_wy);
    // 屏幕像素 → 世界单位比例（Gizmo 拖拽 / 相机平移用）
    f.wpp_x = (2.0f * f.half_w) / img_w;
    f.wpp_y = (2.0f * f.zoom) / img_h;

    // 非编辑模式下 Gizmo 状态强制复位
    if (state != PlayState::Edit) gizmo_.mode = GizmoState::Idle;

    // =======================================================================
    // 编辑模式
    // =======================================================================
    if (state == PlayState::Edit && world) {
        const GizmoMode gizmo_mode = ctx.GetGizmoMode();
        const bool pick_allowed = (gizmo_mode != GizmoMode::Tile &&
                                   gizmo_mode != GizmoMode::Entity);

        // ---- 相机导航：滚轮缩放 + 中键平移 ----
        if (f.hovered) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                float nz = std::clamp(f.zoom * (1.0f - wheel * 0.1f), 16.0f, 4096.0f);
                ctx.SetCameraZoom(nz);
                f.zoom = nz;
                f.half_w = f.zoom * aspect;
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                ImVec2 md = ImGui::GetIO().MouseDelta;
                f.cam_pos.x -= md.x * f.wpp_x;
                f.cam_pos.y += md.y * f.wpp_y;
                ctx.SetCameraPosition(f.cam_pos);
            }
        }

        // ---- 2. 点选实体（左键；Gizmo 悬停/拖拽中不触发）----
        if (pick_allowed && f.hovered && f.mouse_valid &&
            gizmo_.mode == GizmoState::Idle &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            scene::EntityID hit = PickEntityAt(world, f.mouse_wx, f.mouse_wy, scene::kPickRadius);
            if (hit != 0) ctx.SelectEntity(hit);
            else ctx.ClearSelection();
        }

        // ---- 3. 右键点选 + 上下文菜单 + Delete 键（viewport_context_menu.cpp）----
        DrawContextMenu(f);

        // ---- 4. 网格（viewport_gizmo.cpp）----
        DrawGrid(f);

        // ---- 5. 选中实体高亮 + Translate Gizmo（viewport_gizmo.cpp）----
        DrawSelectionGizmo(f);

        // ---- 6. Entity 放置模式（viewport_gizmo.cpp）----
        DrawEntityPlacement(f);

        // ---- 7. Tile/Passage/Terrain 绘制（viewport_tile_editing.cpp）----
        DrawTileEditing(f);

        // ---- 8. 底部浮动工具栏（Gizmo 模式 + ViewMode）----
        {
            constexpr float kBtn = 24.0f;
            constexpr float kPad = 8.0f;
            constexpr float kGap = 4.0f;
            const struct { const char* label; const char* tip; GizmoMode mode; } kGizmoBtns[5] = {
                {"T",         "Translate",   GizmoMode::Translate},
                {"R",         "Rotate",      GizmoMode::Rotate},
                {"S",         "Scale",       GizmoMode::Scale},
                {ICON_FA_TH,   "Tile Paint",  GizmoMode::Tile},
                {ICON_FA_PLUS, "Place Entity", GizmoMode::Entity},
            };
            float bar_w = 5 * kBtn + 4 * kGap + 2 * kPad;
            ImVec2 bar_min(f.img_min.x + kPad, f.img_max.y - kBtn - 2 * kPad);
            f.dl->AddRectFilled(bar_min, ImVec2(bar_min.x + bar_w, bar_min.y + kBtn + kPad),
                                IM_COL32(20, 22, 28, 220), 6.0f);
            ImGui::SetCursorScreenPos(ImVec2(bar_min.x + kPad, bar_min.y + kPad * 0.5f));
            for (const auto& b : kGizmoBtns) {
                if (MG_ToolbarButton(b.label, b.tip, gizmo_mode == b.mode, ImVec2(kBtn, kBtn)))
                    ctx.SetGizmoMode(b.mode);
                ImGui::SameLine(0.0f, kGap);
            }

            // ViewMode（右下）：Lit / Unlit / Wireframe
            auto view_mode = ctx.GetViewMode();
            const struct { const char* label; const char* tip; EditorContext::ViewMode mode; } kViewBtns[3] = {
                {"L", "Lit",       EditorContext::ViewMode::Lit},
                {"U", "Unlit",     EditorContext::ViewMode::Unlit},
                {"W", "Wireframe", EditorContext::ViewMode::Wireframe},
            };
            float vw = 3 * kBtn + 2 * kGap + 2 * kPad;
            ImVec2 vm_min(f.img_max.x - vw - kPad, f.img_max.y - kBtn - 2 * kPad);
            f.dl->AddRectFilled(vm_min, ImVec2(vm_min.x + vw, vm_min.y + kBtn + kPad),
                                IM_COL32(20, 22, 28, 220), 6.0f);
            ImGui::SetCursorScreenPos(ImVec2(vm_min.x + kPad, vm_min.y + kPad * 0.5f));
            for (int i = 0; i < 3; ++i) {
                bool wireframe_off = (kViewBtns[i].mode == EditorContext::ViewMode::Wireframe &&
                                      !ctx.GetViewport().IsWireframeSupported());
                if (wireframe_off) ImGui::BeginDisabled();
                if (MG_ToolbarButton(kViewBtns[i].label, kViewBtns[i].tip,
                                     view_mode == kViewBtns[i].mode, ImVec2(kBtn, kBtn)))
                    ctx.SetViewMode(kViewBtns[i].mode);
                if (wireframe_off) ImGui::EndDisabled();
                if (i < 2) ImGui::SameLine(0.0f, kGap);
            }
        }
    }

    // =======================================================================
    // Play Mode（覆盖层 UI 已提取到 gameui/playmode_ui.*，与 game.exe 共用）
    // =======================================================================
    auto* event_system = ctx.GetEventSystem();
    const bool in_game = (state == PlayState::Playing &&
                          ctx.GetScreenMode() == EditorContext::ScreenMode::Game);

    // Escape 暂停切换已下沉 GameRuntime；此处仅维护子菜单状态复位
    if (state != PlayState::Paused) pause_sub_ = gameui::PauseSub::None;

    // 组装共享 UI 输入
    gameui::PlayUIContext pui;
    pui.es = event_system;
    pui.dbs = &ctx.GetDatabases();
    pui.play_mode = &ctx.GetPlayMode();
    pui.dl = f.dl;
    pui.view_min = f.img_min;
    pui.view_max = f.img_max;
    pui.fade_alpha = ctx.GetFadeAlpha();
    if (const auto* manifest = ctx.GetProjectManifest())
        if (!manifest->name.empty()) pui.game_title = manifest->name.c_str();
    pui.project_root = ctx.GetProjectPath();
    pui.on_exit = [&ctx]() { ctx.SetPlayState(PlayState::Edit); };
    pui.exit_label = "Exit to Editor";
    pui.cam_pos = f.cam_pos;
    pui.cam_zoom = f.zoom;

    // ---- Fade 过渡遮罩 ----
    if (state != PlayState::Edit) gameui::DrawFadeOverlay(pui);

    // ---- 标题画面 ----
    if (state == PlayState::Playing &&
        ctx.GetScreenMode() == EditorContext::ScreenMode::Title) {
        gameui::DrawTitleScreen(pui);
    }

    // ---- 暂停菜单 ----
    if (state == PlayState::Paused) gameui::DrawPauseMenu(pui, pause_sub_);

    // ---- Toast / 对话框 / 选项 / 商店 / 战斗 / HUD ----
    if (in_game) {
        gameui::DrawToasts(pui);
        gameui::DrawDialog(pui, f.mouse);
        gameui::DrawChoices(pui);
        gameui::DrawShopOverlay(pui);
        gameui::DrawBattleOverlay(pui);
        gameui::DrawHUDOverlays(pui);
    }

    // ---- 9. Undo flush：帧内收集的绘制变化推入命令栈（viewport_tile_editing.cpp）----
    FlushPaintCommands(world);

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
