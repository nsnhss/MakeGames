// viewport_panel_playmode.cpp — Play Mode 覆盖层 UI（商店 / 战斗），
// 从 viewport_panel.cpp 拆分，保持成员语义不变。
#include "viewport_panel.h"
#include "src/editor/editor_context.h"
#include "scene/event_system.h"
#include "resource/skill_database.h"
#include "resource/item_database.h"
#include "imgui.h"
#include <algorithm>
#include <string>

namespace mg {
namespace editor {

static void DrawHealthBar(ImDrawList* dl, ImVec2 tl, ImVec2 br, int cur, int max, ImU32 col) {
    float ratio = max > 0 ? std::clamp((float)cur / (float)max, 0.0f, 1.0f) : 0.0f;
    dl->AddRectFilled(tl, br, IM_COL32(40, 40, 40, 255), 4.0f);
    ImVec2 mid(br.x - (br.x - tl.x) * (1.0f - ratio), br.y);
    dl->AddRectFilled(tl, mid, col, 4.0f);
    std::string label = std::to_string(cur) + "/" + std::to_string(max);
    ImVec2 ts = ImGui::CalcTextSize(label.c_str());
    dl->AddText(ImVec2(tl.x + (br.x - tl.x - ts.x) * 0.5f, tl.y + 1),
                IM_COL32(255, 255, 255, 255), label.c_str());
}

// Play Mode — Shop 覆盖层
void ViewportPanel::DrawShopOverlay() {
    auto& ctx = ctx_;
    if (ctx.GetPlayState() != EditorContext::PlayState::Playing) return;
    auto* event_system = ctx.GetEventSystem();
    if (!event_system || !event_system->IsShopActive()) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 vp_pos = ImGui::GetWindowPos();
    ImVec2 vp_size = ImGui::GetWindowSize();

    // 背景遮罩
    dl->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y),
        IM_COL32(0, 0, 0, 160));

    float box_w = std::min(vp_size.x * 0.7f, 480.0f);
    float row_h = 28.0f;
    const std::string shop_id = event_system->GetPendingShopId();
    const auto* shop_def = ctx.GetShopDatabase().Find(shop_id);
    const auto& entries = shop_def ? shop_def->items : std::vector<mg::ShopDatabase::ShopItem>{};

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
    int gold = event_system->GetVariableInt("gold");
    std::string gold_text = "Gold: " + std::to_string(gold);
    ImVec2 gt_size = ImGui::CalcTextSize(gold_text.c_str());
    ImGui::SetCursorScreenPos(ImVec2(box_pos.x + box_w - gt_size.x - 18, box_pos.y + 16));
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", gold_text.c_str());

    // 商品列表
    float y = box_pos.y + header_h + 6;
    const auto& item_db = ctx.GetItemDatabase();
    for (const auto& entry : entries) {
        const auto* item_def = item_db.Find(entry.item_id);
        std::string display_name = item_def ? item_def->name : entry.item_id;
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
            event_system->BuyItem(entry.item_id, entry.price);
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
        event_system->CloseShop();
    }
}

// Play Mode — Battle 覆盖层
void ViewportPanel::DrawBattleOverlay() {
    auto& ctx = ctx_;
    if (ctx.GetPlayState() != EditorContext::PlayState::Playing) return;
    auto* event_system = ctx.GetEventSystem();
    if (!event_system || !event_system->IsBattleActive()) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 vp_pos = ImGui::GetWindowPos();
    ImVec2 vp_size = ImGui::GetWindowSize();

    // 背景遮罩
    dl->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y),
        IM_COL32(0, 0, 0, 180));

    auto state = event_system->GetBattleState();
    const auto& player = event_system->GetBattlePlayer();
    const auto& enemies = event_system->GetBattleEnemies();
    int target_idx = event_system->GetBattleTarget();
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
            if (ImGui::Button(enemy.name.c_str())) event_system->SetBattleTarget(i);
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
        event_system->PlayerAttack();
    }
    ImGui::SameLine();
    if (ImGui::Button("Skill", ImVec2(70, 26))) {
        ImGui::OpenPopup("skill_popup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Item", ImVec2(70, 26))) {
        event_system->PlayerUseItem();
    }
    ImGui::SameLine();
    if (ImGui::Button("Escape", ImVec2(70, 26))) {
        event_system->PlayerTryEscape();
    }
    if (!player_turn) ImGui::EndDisabled();

    // Skill 选择弹出菜单
    if (ImGui::BeginPopup("skill_popup")) {
        const auto& skill_db = ctx.GetSkillDatabase();
        bool any = false;
        for (const auto& [sid, skill] : skill_db.skills) {
            bool enough = player.mp >= skill.mp_cost;
            if (!enough) ImGui::BeginDisabled();
            if (ImGui::Selectable((skill.name + " (" + std::to_string(skill.mp_cost) + " MP)").c_str())) {
                event_system->PlayerUseSkill(skill.id);
            }
            if (!enough) ImGui::EndDisabled();
            any = true;
        }
        if (!any) ImGui::Text("No skills available");
        ImGui::EndPopup();
    }

    // 战斗日志
    const std::string& log = event_system->GetBattleLog();
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

} // namespace editor
} // namespace mg
