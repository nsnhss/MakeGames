// playmode_ui.cpp — Play Mode 覆盖层 UI 实现（从 viewport_panel(.playmode).cpp 机械提取）
#include "gameui/playmode_ui.h"
#include "scene/event_system.h"
#include "scene/play_mode_controller.h"
#include "resource/game_databases.h"
#include "resource/actor_database.h"
#include "core/log.h"
#include <algorithm>
#include <string>
#include <vector>

namespace mg {
namespace gameui {

namespace {

float ViewW(const PlayUIContext& ui) { return ui.view_max.x - ui.view_min.x; }
float ViewH(const PlayUIContext& ui) { return ui.view_max.y - ui.view_min.y; }

// 世界坐标 → 屏幕坐标（与 editor ViewportPanel::WorldToScreen 同公式：
// ortho_size=半高、Y-up、zoom=ortho_size）
ImVec2 WorldToScreen(const PlayUIContext& ui, float wx, float wy) {
    float vp_w = ViewW(ui), vp_h = ViewH(ui);
    float aspect = vp_h > 0.0f ? vp_w / vp_h : 1.0f;
    float half_w = ui.cam_zoom * aspect;
    float u = half_w > 0.0f ? ((wx - ui.cam_pos.x) / half_w + 1.0f) * 0.5f : 0.5f;
    float v = ui.cam_zoom > 0.0f ? (-(wy - ui.cam_pos.y) / ui.cam_zoom + 1.0f) * 0.5f : 0.5f;
    return ImVec2(ui.view_min.x + u * vp_w, ui.view_min.y + v * vp_h);
}

void DrawHealthBar(ImDrawList* dl, ImVec2 tl, ImVec2 br, int cur, int max, ImU32 col) {
    float ratio = max > 0 ? std::clamp((float)cur / (float)max, 0.0f, 1.0f) : 0.0f;
    dl->AddRectFilled(tl, br, IM_COL32(40, 40, 40, 255), 4.0f);
    ImVec2 mid(br.x - (br.x - tl.x) * (1.0f - ratio), br.y);
    dl->AddRectFilled(tl, mid, col, 4.0f);
    std::string label = std::to_string(cur) + "/" + std::to_string(max);
    ImVec2 ts = ImGui::CalcTextSize(label.c_str());
    dl->AddText(ImVec2(tl.x + (br.x - tl.x - ts.x) * 0.5f, tl.y + 1),
                IM_COL32(255, 255, 255, 255), label.c_str());
}

// ---- D6-1：屏幕空间 Toast（Log Warn/Error 的 Play Mode 可见通道）----
// 纹理/音频/脚本加载失败等 MG_LOG_WARN/ERROR 原先只进编辑器 Console，
// Play Mode 玩家零感知。这里挂一个全局 Log sink，把 Warn/Error 转为
// 屏幕顶部渐隐 toast；相同文本连续出现合并计数，上限 6 条防刷屏。
struct ScreenToast {
    std::string text;
    bool is_error = false;
    float age = 0.0f;
    int count = 1;
};
constexpr float kScreenToastLife = 4.0f;

std::vector<ScreenToast>& ScreenToasts() {
    static std::vector<ScreenToast> v;
    return v;
}

void EnsureScreenToastSink() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    core::Log::AddSink([](const core::LogMessage& m) {
        if (m.level != core::LogLevel::Warn && m.level != core::LogLevel::Error) return;
        auto& v = ScreenToasts();
        const bool is_err = (m.level == core::LogLevel::Error);
        if (!v.empty() && v.back().text == m.text && v.back().is_error == is_err) {
            v.back().count += 1;
            v.back().age = 0.0f;
            return;
        }
        v.push_back({m.text, is_err, 0.0f, 1});
        if (v.size() > 6) v.erase(v.begin());
    });
}

} // namespace

// ---- Fade 过渡遮罩 ----
void DrawFadeOverlay(const PlayUIContext& ui) {
    if (ui.fade_alpha <= 0.0f || !ui.dl) return;
    int alpha = (int)(std::clamp(ui.fade_alpha, 0.0f, 1.0f) * 255.0f);
    ui.dl->AddRectFilled(ui.view_min, ui.view_max, IM_COL32(0, 0, 0, alpha));
}

// ---- 标题画面 ----
void DrawTitleScreen(const PlayUIContext& ui) {
    if (!ui.dl || !ui.play_mode) return;
    float img_w = ViewW(ui), img_h = ViewH(ui);
    ui.dl->AddRectFilled(ui.view_min, ui.view_max, IM_COL32(10, 12, 20, 235));

    const char* title = (ui.game_title && ui.game_title[0]) ? ui.game_title : "MakeGames";
    ImVec2 ts = ImGui::CalcTextSize(title);
    float title_x = ui.view_min.x + (img_w - ts.x * 2.0f) * 0.5f;
    // 放大标题（用两枚阴影偏移模拟加重）
    ImGui::SetCursorScreenPos(ImVec2(title_x, ui.view_min.y + img_h * 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    constexpr float kBtnW = 200.0f;
    constexpr float kBtnH = 34.0f;
    float bx = ui.view_min.x + (img_w - kBtnW) * 0.5f;
    float by = ui.view_min.y + img_h * 0.45f;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (ImGui::Button("Start New Game", ImVec2(kBtnW, kBtnH)))
        ui.play_mode->RequestStartNewGame();
    ImGui::SetCursorScreenPos(ImVec2(bx, by + kBtnH + 10.0f));
    if (ImGui::Button("Continue", ImVec2(kBtnW, kBtnH)))
        ui.play_mode->RequestContinueGame();
    ImGui::SetCursorScreenPos(ImVec2(bx, by + 2 * (kBtnH + 10.0f)));
    if (ImGui::Button(ui.exit_label, ImVec2(kBtnW, kBtnH)) && ui.on_exit)
        ui.on_exit();
}

// ---- 暂停菜单 ----
void DrawPauseMenu(const PlayUIContext& ui, PauseSub& sub) {
    if (!ui.dl || !ui.play_mode) return;
    auto* dl = ui.dl;
    float img_w = ViewW(ui), img_h = ViewH(ui);
    dl->AddRectFilled(ui.view_min, ui.view_max, IM_COL32(0, 0, 0, 150));

    constexpr float kBoxW = 260.0f;
    float box_h = 0.0f;
    switch (sub) {
        case PauseSub::None:      box_h = 6 * 34.0f + 40.0f; break;
        case PauseSub::SaveSlots:
        case PauseSub::LoadSlots: box_h = 4 * 34.0f + 44.0f; break;
        case PauseSub::Items:
        case PauseSub::Status:    box_h = 240.0f; break;
    }
    ImVec2 bp(ui.view_min.x + (img_w - kBoxW) * 0.5f, ui.view_min.y + (img_h - box_h) * 0.5f);
    dl->AddRectFilled(bp, ImVec2(bp.x + kBoxW, bp.y + box_h), IM_COL32(25, 30, 45, 245), 10.0f);
    dl->AddRect(bp, ImVec2(bp.x + kBoxW, bp.y + box_h), IM_COL32(100, 110, 140, 255), 10.0f);

    float bx = bp.x + 20.0f;
    float bw = kBoxW - 40.0f;
    float by = bp.y + 16.0f;
    auto menu_button = [&](const char* label) {
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        by += 34.0f;
        return ImGui::Button(label, ImVec2(bw, 28.0f));
    };

    if (sub == PauseSub::None) {
        if (menu_button("Resume")) ui.play_mode->SetState(scene::PlayModeController::State::Playing);
        if (menu_button("Save"))   sub = PauseSub::SaveSlots;
        if (menu_button("Load"))   sub = PauseSub::LoadSlots;
        if (menu_button("Items"))  sub = PauseSub::Items;
        if (menu_button("Status")) sub = PauseSub::Status;
        if (menu_button(ui.exit_label) && ui.on_exit) ui.on_exit();
    } else if (sub == PauseSub::SaveSlots || sub == PauseSub::LoadSlots) {
        const bool is_save = (sub == PauseSub::SaveSlots);
        for (int slot = 1; slot <= 3; ++slot) {
            auto info = ui.play_mode->ReadSaveSlotInfo(ui.project_root, slot);
            std::string label = "Slot " + std::to_string(slot);
            if (info.exists) {
                label += "  " + info.scene_name;
                if (!info.last_save_time.empty())
                    label += "  (" + info.last_save_time + ")";
            } else {
                label += "  (Empty)";
            }
            bool clickable = is_save || info.exists;
            if (!clickable) ImGui::BeginDisabled();
            if (menu_button(label.c_str())) {
                if (is_save) ui.play_mode->RequestSaveToSlot(slot);
                else         ui.play_mode->RequestLoadFromSlot(slot);
                sub = PauseSub::None;
            }
            if (!clickable) ImGui::EndDisabled();
        }
        if (menu_button("Back")) sub = PauseSub::None;
    } else if (sub == PauseSub::Items) {
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Items");
        by += 26.0f;
        if (ui.es) {
            auto items = ui.es->GetInventoryItems();
            for (const auto& [id, count] : items) {
                std::string name = id;
                if (ui.dbs) {
                    if (const auto* def = ui.dbs->item.Find(id)) name = def->name;
                }
                ImGui::SetCursorScreenPos(ImVec2(bx, by));
                ImGui::Text("%s x%d", name.c_str(), count);
                by += 22.0f;
            }
            if (items.empty()) {
                ImGui::SetCursorScreenPos(ImVec2(bx, by));
                ImGui::TextDisabled("(Empty)");
                by += 22.0f;
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(bx, bp.y + box_h - 38.0f));
        if (ImGui::Button("Back", ImVec2(bw, 28.0f))) sub = PauseSub::None;
    } else { // Status（D6-4：角色实时数值，EvalActorBattleStats 与战斗同公式）
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Status");
        by += 28.0f;

        // 主角约定与战斗一致：缺省 actor id = "Player"，找不到则退到数据库首个角色
        const ActorDatabase::ActorDef* def = nullptr;
        if (ui.dbs) {
            def = ui.dbs->actor.Find("Player");
            if (!def && !ui.dbs->actor.actors.empty())
                def = &ui.dbs->actor.actors.begin()->second;
        }
        int level = ui.es ? ui.es->GetVariableInt("level") : 0;
        int gold = ui.es ? ui.es->GetVariableInt("gold") : 0;

        if (def) {
            if (level <= 0) level = 1;
            ActorBattleStats st = EvalActorBattleStats(*def, level);
            ImGui::SetCursorScreenPos(ImVec2(bx, by));
            ImGui::Text("%s   Lv %d", def->name.c_str(), level);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 24));  ImGui::Text("HP:  %d", st.hp);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 48));  ImGui::Text("MP:  %d", st.mp);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 72));  ImGui::Text("ATK: %d", st.atk);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 96));  ImGui::Text("DEF: %d", st.def);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 124)); ImGui::Text("Gold: %d", gold);
        } else {
            // 无角色数据时退化为变量占位（原行为）
            int hp = ui.es ? ui.es->GetVariableInt("hp") : 0;
            int mp = ui.es ? ui.es->GetVariableInt("mp") : 0;
            ImGui::SetCursorScreenPos(ImVec2(bx, by));        ImGui::Text("HP:   %d", hp);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 24));   ImGui::Text("MP:   %d", mp);
            ImGui::SetCursorScreenPos(ImVec2(bx, by + 48));   ImGui::Text("Gold: %d", gold);
        }
        ImGui::SetCursorScreenPos(ImVec2(bx, bp.y + box_h - 38.0f));
        if (ImGui::Button("Back", ImVec2(bw, 28.0f))) sub = PauseSub::None;
    }
}

// ---- 对话框 ----
void DrawDialog(const PlayUIContext& ui, ImVec2 mouse) {
    if (!ui.dl || !ui.es || !ui.es->HasPendingMessage()) return;
    auto* dl = ui.dl;
    float img_w = ViewW(ui);
    float box_w = img_w - 32.0f;
    float box_h = 110.0f;
    ImVec2 bp(ui.view_min.x + 16.0f, ui.view_max.y - box_h - 16.0f);
    dl->AddRectFilled(bp, ImVec2(bp.x + box_w, bp.y + box_h), IM_COL32(20, 24, 36, 235), 8.0f);
    dl->AddRect(bp, ImVec2(bp.x + box_w, bp.y + box_h), IM_COL32(90, 100, 130, 255), 8.0f);

    float text_y = bp.y + 12.0f;
    const std::string& speaker_id = ui.es->GetPendingMessageName();
    if (!speaker_id.empty()) {
        std::string speaker = speaker_id;
        if (ui.dbs) {
            if (const auto* actor = ui.dbs->actor.Find(speaker_id))
                speaker = actor->name;
        }
        ImGui::SetCursorScreenPos(ImVec2(bp.x + 14.0f, bp.y + 8.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", speaker.c_str());
        ImVec2 ns = ImGui::CalcTextSize(speaker.c_str());
        dl->AddLine(ImVec2(bp.x + 14.0f, bp.y + 28.0f),
                    ImVec2(bp.x + 14.0f + ns.x, bp.y + 28.0f),
                    IM_COL32(1.0f * 255, 0.85f * 255, 0.3f * 255, 160));
        text_y = bp.y + 34.0f;
    }
    ImGui::SetCursorScreenPos(ImVec2(bp.x + 14.0f, text_y));
    ImGui::PushTextWrapPos(bp.x + box_w - 14.0f);
    ImGui::TextUnformatted(ui.es->GetPendingMessage().c_str());
    ImGui::PopTextWrapPos();

    // 继续提示
    ImGui::SetCursorScreenPos(ImVec2(bp.x + box_w - 60.0f, bp.y + box_h - 22.0f));
    ImGui::TextDisabled("[Enter]");

    bool clicked_in_box = ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        mouse.x >= bp.x && mouse.x <= bp.x + box_w &&
        mouse.y >= bp.y && mouse.y <= bp.y + box_h;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || clicked_in_box)
        ui.es->AcknowledgeMessage();
}

// ---- 选项 ----
void DrawChoices(const PlayUIContext& ui) {
    if (!ui.dl || !ui.es || !ui.es->IsChoiceActive()) return;
    auto* dl = ui.dl;
    const auto& choices = ui.es->GetChoices();
    float cw = 220.0f;
    float ch = (float)choices.size() * 30.0f + 16.0f;
    if (!ui.es->GetChoiceLabel().empty()) ch += 24.0f;
    ImVec2 cp(ui.view_max.x - cw - 24.0f, ui.view_max.y - ch - 140.0f);
    dl->AddRectFilled(cp, ImVec2(cp.x + cw, cp.y + ch), IM_COL32(20, 24, 36, 240), 8.0f);
    dl->AddRect(cp, ImVec2(cp.x + cw, cp.y + ch), IM_COL32(90, 100, 130, 255), 8.0f);
    float cy = cp.y + 8.0f;
    if (!ui.es->GetChoiceLabel().empty()) {
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 12.0f, cy));
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s",
                           ui.es->GetChoiceLabel().c_str());
        cy += 24.0f;
    }
    for (int i = 0; i < (int)choices.size(); ++i) {
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 12.0f, cy));
        ImGui::PushID(i);
        if (ImGui::Button(choices[i].c_str(), ImVec2(cw - 24.0f, 26.0f)))
            ui.es->AcknowledgeChoice(i);
        ImGui::PopID();
        cy += 30.0f;
    }
}

// ---- Toast 浮动提示 ----
void DrawToasts(const PlayUIContext& ui) {
    if (!ui.dl) return;
    // 实体头顶的世界锚定 toast（EventSystem 事件动作驱动）
    if (ui.es) {
        for (const auto& toast : ui.es->GetToasts()) {
            float a = std::clamp(toast.timer / 0.5f, 0.0f, 1.0f);
            ImVec2 p = WorldToScreen(ui, toast.world_pos.x, toast.world_pos.y + 24.0f);
            ImVec2 ts = ImGui::CalcTextSize(toast.text.c_str());
            p.x -= ts.x * 0.5f;
            ui.dl->AddText(ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, (int)(180 * a)),
                           toast.text.c_str());
            ui.dl->AddText(p, IM_COL32(255, 240, 180, (int)(255 * a)), toast.text.c_str());
        }
    }

    // D6-1：屏幕顶部系统 toast（Log Warn/Error → Play Mode 可见）
    EnsureScreenToastSink();
    auto& toasts = ScreenToasts();
    float dt = ImGui::GetIO().DeltaTime;
    float ty = ui.view_min.y + 8.0f;
    for (size_t i = 0; i < toasts.size();) {
        auto& t = toasts[i];
        t.age += dt;
        if (t.age >= kScreenToastLife) { toasts.erase(toasts.begin() + i); continue; }
        float a = std::clamp((kScreenToastLife - t.age) / 0.5f, 0.0f, 1.0f);
        std::string text = t.count > 1
            ? t.text + "  (x" + std::to_string(t.count) + ")" : t.text;
        ImVec2 ts = ImGui::CalcTextSize(text.c_str());
        ImVec2 p(ui.view_min.x + (ViewW(ui) - ts.x) * 0.5f, ty);
        ImU32 col = t.is_error ? IM_COL32(255, 110, 110, (int)(255 * a))
                               : IM_COL32(255, 210, 120, (int)(255 * a));
        ui.dl->AddText(ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, (int)(180 * a)), text.c_str());
        ui.dl->AddText(p, col, text.c_str());
        ty += ts.y + 4.0f;
        ++i;
    }
}

// ---- 商店覆盖层 ----
void DrawShopOverlay(const PlayUIContext& ui) {
    if (!ui.dl || !ui.es || !ui.es->IsShopActive()) return;
    auto* dl = ui.dl;
    ImVec2 vp_pos = ui.view_min;
    ImVec2 vp_size(ViewW(ui), ViewH(ui));

    // 背景遮罩
    dl->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y),
        IM_COL32(0, 0, 0, 160));

    float box_w = std::min(vp_size.x * 0.7f, 480.0f);
    float row_h = 28.0f;
    const std::string shop_id = ui.es->GetPendingShopId();
    const ShopDatabase::ShopDef* shop_def = ui.dbs ? ui.dbs->shop.Find(shop_id) : nullptr;
    const auto& entries = shop_def ? shop_def->items : std::vector<ShopDatabase::ShopItem>{};

    float header_h = 50.0f;
    float footer_h = 46.0f;
    float list_h = entries.empty() ? 40.0f : entries.size() * (row_h + 6.0f) + 12.0f;
    float box_h = header_h + list_h + footer_h;
    ImVec2 box_pos(vp_pos.x + (vp_size.x - box_w) * 0.5f,
                   vp_pos.y + (vp_size.y - box_h) * 0.45f);
    dl->AddRectFilled(box_pos, ImVec2(box_pos.x + box_w, box_pos.y + box_h),
        IM_COL32(25, 30, 45, 245), 10.0f);
    dl->AddRect(box_pos, ImVec2(box_pos.x + box_w, box_pos.y + box_h),
        IM_COL32(100, 110, 140, 255), 10.0f);

    // 标题
    ImGui::SetCursorScreenPos(ImVec2(box_pos.x + 18, box_pos.y + 14));
    ImGui::TextColored(ImVec4(1, 1, 1, 0.95f), "Shop");

    // 金币
    int gold = ui.es->GetVariableInt("gold");
    std::string gold_text = "Gold: " + std::to_string(gold);
    ImVec2 gt_size = ImGui::CalcTextSize(gold_text.c_str());
    ImGui::SetCursorScreenPos(ImVec2(box_pos.x + box_w - gt_size.x - 18, box_pos.y + 16));
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", gold_text.c_str());

    // 商品列表
    float y = box_pos.y + header_h + 6;
    for (const auto& entry : entries) {
        std::string display_name = entry.item_id;
        if (ui.dbs) {
            if (const auto* item_def = ui.dbs->item.Find(entry.item_id))
                display_name = item_def->name;
        }
        std::string price_text = std::to_string(entry.price) + " G";

        ImGui::SetCursorScreenPos(ImVec2(box_pos.x + 18, y));
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", display_name.c_str());

        ImVec2 pt_size = ImGui::CalcTextSize(price_text.c_str());
        ImGui::SetCursorScreenPos(ImVec2(box_pos.x + box_w - pt_size.x - 86, y + 2));
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", price_text.c_str());

        ImGui::SetCursorScreenPos(ImVec2(box_pos.x + box_w - 76, y));
        bool can_afford = gold >= entry.price;
        if (!can_afford) ImGui::BeginDisabled();
        if (ImGui::Button(("Buy##" + entry.item_id).c_str(), ImVec2(60, row_h))) {
            ui.es->BuyItem(entry.item_id, entry.price);
        }
        if (!can_afford) ImGui::EndDisabled();

        y += row_h + 6;
    }

    if (entries.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(box_pos.x + 18, y));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No items available.");
    }

    // 关闭按钮
    ImGui::SetCursorScreenPos(ImVec2(box_pos.x + (box_w - 100) * 0.5f, box_pos.y + box_h - footer_h + 6));
    if (ImGui::Button("Close", ImVec2(100, 28))) {
        ui.es->CloseShop();
    }
}

// ---- 战斗覆盖层 ----
void DrawBattleOverlay(const PlayUIContext& ui) {
    if (!ui.dl || !ui.es || !ui.es->IsBattleActive()) return;
    auto* dl = ui.dl;
    ImVec2 vp_pos = ui.view_min;
    ImVec2 vp_size(ViewW(ui), ViewH(ui));

    // 背景遮罩
    dl->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y),
        IM_COL32(0, 0, 0, 180));

    auto state = ui.es->GetBattleState();
    const auto& player = ui.es->GetBattlePlayer();
    const auto& enemies = ui.es->GetBattleEnemies();
    int target_idx = ui.es->GetBattleTarget();
    bool player_turn = (state == scene::EventSystem::BattleState::PlayerTurn);

    // 顶部敌人面板（多敌人列表）
    float panel_w = std::min(vp_size.x * 0.7f, 420.0f);
    float slot_h = 64.0f;
    float start_y = vp_pos.y + 40;
    for (int i = 0; i < (int)enemies.size(); ++i) {
        const auto& enemy = enemies[i];
        float slot_w = panel_w;
        ImVec2 ep(vp_pos.x + (vp_size.x - slot_w) * 0.5f, start_y + i * (slot_h + 8));
        bool selected = (i == target_idx);
        ImU32 border = selected ? IM_COL32(255, 220, 80, 255) : IM_COL32(180, 80, 80, 255);
        dl->AddRectFilled(ep, ImVec2(ep.x + slot_w, ep.y + slot_h),
            IM_COL32(40, 30, 30, 220), 8.0f);
        dl->AddRect(ep, ImVec2(ep.x + slot_w, ep.y + slot_h), border, 8.0f);
        ImGui::SetCursorScreenPos(ImVec2(ep.x + 10, ep.y + 6));
        bool alive = enemy.hp > 0;
        ImVec4 name_col = alive ? ImVec4(1, 0.7f, 0.7f, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1);
        if (alive && player_turn) {
            ImGui::PushID(i);
            if (ImGui::Button(enemy.name.c_str())) ui.es->SetBattleTarget(i);
            ImGui::PopID();
        } else {
            ImGui::TextColored(name_col, "%s", enemy.name.c_str());
        }
        if (!alive) {
            ImVec2 ds = ImGui::CalcTextSize("DEFEATED");
            ImGui::SameLine(slot_w - ds.x - 12);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "DEFEATED");
        }
        DrawHealthBar(dl, ImVec2(ep.x + 10, ep.y + 34),
                      ImVec2(ep.x + slot_w - 10, ep.y + 52),
                      enemy.hp, enemy.max_hp, IM_COL32(220, 60, 60, 255));
    }

    // 底部玩家面板
    ImVec2 player_pos(vp_pos.x + (vp_size.x - panel_w) * 0.5f, vp_pos.y + vp_size.y - 180);
    dl->AddRectFilled(player_pos, ImVec2(player_pos.x + panel_w, player_pos.y + 120),
        IM_COL32(30, 30, 40, 220), 8.0f);
    dl->AddRect(player_pos, ImVec2(player_pos.x + panel_w, player_pos.y + 120),
        IM_COL32(80, 120, 180, 255), 8.0f);
    ImGui::SetCursorScreenPos(ImVec2(player_pos.x + 14, player_pos.y + 10));
    ImGui::TextColored(ImVec4(0.7f, 0.85f, 1, 1), "%s", player.name.c_str());
    DrawHealthBar(dl, ImVec2(player_pos.x + 14, player_pos.y + 42),
                  ImVec2(player_pos.x + panel_w - 14, player_pos.y + 60),
                  player.hp, player.max_hp, IM_COL32(60, 180, 90, 255));
    DrawHealthBar(dl, ImVec2(player_pos.x + 14, player_pos.y + 68),
                  ImVec2(player_pos.x + panel_w - 14, player_pos.y + 86),
                  player.mp, player.max_mp, IM_COL32(60, 120, 220, 255));

    // 行动按钮（仅 PlayerTurn）
    ImGui::SetCursorScreenPos(ImVec2(player_pos.x + 14, player_pos.y + 94));
    if (!player_turn) ImGui::BeginDisabled();
    if (ImGui::Button("Attack", ImVec2(70, 26))) {
        ui.es->PlayerAttack();
    }
    ImGui::SameLine();
    if (ImGui::Button("Skill", ImVec2(70, 26))) {
        ImGui::OpenPopup("skill_popup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Item", ImVec2(70, 26))) {
        ui.es->PlayerUseItem();
    }
    ImGui::SameLine();
    if (ImGui::Button("Escape", ImVec2(70, 26))) {
        ui.es->PlayerTryEscape();
    }
    if (!player_turn) ImGui::EndDisabled();

    // Skill 选择弹出菜单
    if (ImGui::BeginPopup("skill_popup")) {
        bool any = false;
        if (ui.dbs) {
            for (const auto& [sid, skill] : ui.dbs->skill.skills) {
                bool enough = player.mp >= skill.mp_cost;
                if (!enough) ImGui::BeginDisabled();
                if (ImGui::Selectable((skill.name + " (" + std::to_string(skill.mp_cost) + " MP)").c_str())) {
                    ui.es->PlayerUseSkill(skill.id);
                }
                if (!enough) ImGui::EndDisabled();
                any = true;
            }
        }
        if (!any) ImGui::Text("No skills available");
        ImGui::EndPopup();
    }

    // 战斗日志
    const std::string& log = ui.es->GetBattleLog();
    ImVec2 ts = ImGui::CalcTextSize(log.c_str(), nullptr, false, panel_w - 28);
    ImGui::SetCursorScreenPos(ImVec2(vp_pos.x + (vp_size.x - ts.x) * 0.5f,
                                     vp_pos.y + vp_size.y * 0.55f));
    ImGui::PushTextWrapPos(vp_pos.x + (vp_size.x + panel_w - 28) * 0.5f);
    ImGui::TextColored(ImVec4(1, 1, 1, 0.95f), "%s", log.c_str());
    ImGui::PopTextWrapPos();

    // 胜利 / 失败大字
    if (state == scene::EventSystem::BattleState::Victory) {
        ImVec2 vs = ImGui::CalcTextSize("VICTORY");
        ImGui::SetCursorScreenPos(ImVec2(vp_pos.x + (vp_size.x - vs.x) * 0.5f, vp_pos.y + vp_size.y * 0.35f));
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "VICTORY");
    } else if (state == scene::EventSystem::BattleState::Defeat) {
        ImVec2 vs = ImGui::CalcTextSize("DEFEAT");
        ImGui::SetCursorScreenPos(ImVec2(vp_pos.x + (vp_size.x - vs.x) * 0.5f, vp_pos.y + vp_size.y * 0.35f));
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "DEFEAT");
    }
}

// ---- HUD overlays（status 左上 / inventory 左下 / title 顶部居中）----
void DrawHUDOverlays(const PlayUIContext& ui) {
    if (!ui.dl || !ui.es) return;
    auto* dl = ui.dl;
    float img_w = ViewW(ui);
    float stack_y = ui.view_min.y + 12.0f; // 未知名称依次下排
    for (const auto& [name, ov] : ui.es->GetHUDOverlays()) {
        if (!ov.visible) continue;

        // 内容行
        std::vector<std::string> lines;
        if (!ov.text.empty()) lines.push_back(ov.text);
        if (name == "inventory") {
            for (const auto& [id, count] : ui.es->GetInventoryItems()) {
                std::string item_name = id;
                if (ui.dbs) {
                    if (const auto* def = ui.dbs->item.Find(id)) item_name = def->name;
                }
                lines.push_back(item_name + " x" + std::to_string(count));
            }
        }
        if (lines.empty()) continue;

        float max_w = 0.0f;
        for (const auto& l : lines)
            max_w = std::max(max_w, ImGui::CalcTextSize(l.c_str()).x);
        float box_w = max_w + 20.0f;
        float box_h = (float)lines.size() * 20.0f + 12.0f;

        ImVec2 bp;
        if (name == "inventory")
            bp = ImVec2(ui.view_min.x + 12.0f, ui.view_max.y - box_h - 12.0f);
        else if (name == "title")
            bp = ImVec2(ui.view_min.x + (img_w - box_w) * 0.5f, ui.view_min.y + 12.0f);
        else if (name == "status")
            bp = ImVec2(ui.view_min.x + 12.0f, ui.view_min.y + 12.0f);
        else {
            bp = ImVec2(ui.view_min.x + 12.0f, stack_y);
            stack_y += box_h + 8.0f;
        }

        dl->AddRectFilled(bp, ImVec2(bp.x + box_w, bp.y + box_h),
                          IM_COL32(0, 0, 0, 140), 6.0f);
        for (size_t i = 0; i < lines.size(); ++i) {
            dl->AddText(ImVec2(bp.x + 10.0f, bp.y + 6.0f + (float)i * 20.0f),
                        IM_COL32(255, 255, 255, 230), lines[i].c_str());
        }
    }
}

} // namespace gameui
} // namespace mg
