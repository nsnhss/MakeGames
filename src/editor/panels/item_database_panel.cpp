#include "item_database_panel.h"
#include "resource/game_databases.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_panel.h"
#include "src/editor/imgui/mg_imgui_extensions.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace mg {
namespace editor {

namespace {

int InputTextStringCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufSize);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextString(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    bool changed = ImGui::InputText(label, str->data(), str->capacity() + 1, flags, InputTextStringCallback, str);
    if (changed) str->resize(strlen(str->c_str()));
    return changed;
}

std::string GenerateNextId(const std::string& prefix, auto& map) {
    int max_num = 0;
    for (const auto& [id, _] : map) {
        if (id.rfind(prefix, 0) == 0 && id.size() > prefix.size()) {
            try {
                int num = std::stoi(id.substr(prefix.size()));
                max_num = std::max(max_num, num);
            } catch (...) {}
        }
    }
    return prefix + std::to_string(max_num + 1);
}

} // namespace

const char* ItemDatabasePanel::GetIcon() const {
    return ICON_FA_BOOK;
}

void ItemDatabasePanel::Draw() {
    if (!MG_BeginPanel("Item Database", ICON_FA_BOOK, &is_open_,
                       ImGuiWindowFlags_NoDocking)) { MG_EndPanel(); return; }


    if (ImGui::BeginTabBar("DatabaseTabs")) {
        // ---------- Items ----------
        if (ImGui::BeginTabItem("Items")) {
            auto& db = databases_.item;
            if (ImGui::Button(ICON_FA_PLUS " Add Item")) {
                std::string new_id = GenerateNextId("Item", db.items);
                auto& def = db.items[new_id];
                def.id = new_id; def.name = "New Item"; def.description = ""; def.price = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_SAVE " Save")) if (on_save_) on_save_();
            ImGui::Separator();

            std::vector<std::string> ids;
            for (const auto& [id, _] : db.items) ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            std::string to_delete;
            if (ImGui::BeginTable("ItemTable", 4,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();
                for (const auto& id : ids) {
                    auto& def = db.items[id];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Selectable((id + "##sel_" + id).c_str(), false,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                    if (ImGui::BeginPopupContextItem() && ImGui::MenuItem("Delete")) to_delete = id;
                    ImGui::TableSetColumnIndex(1); InputTextString(("##name_" + id).c_str(), &def.name);
                    ImGui::TableSetColumnIndex(2); InputTextString(("##desc_" + id).c_str(), &def.description);
                    ImGui::TableSetColumnIndex(3); ImGui::InputInt(("##price_" + id).c_str(), &def.price);
                }
                ImGui::EndTable();
            }
            if (!to_delete.empty()) db.items.erase(to_delete);
            ImGui::EndTabItem();
        }

        // ---------- Actors ----------
        if (ImGui::BeginTabItem("Actors")) {
            auto& db = databases_.actor;
            if (ImGui::Button(ICON_FA_PLUS " Add Actor")) {
                std::string new_id = GenerateNextId("Actor", db.actors);
                auto& def = db.actors[new_id];
                def.id = new_id; def.name = "New Actor"; def.hp = def.mp = def.atk = def.def = 0; def.sprite = "";
                // 成长曲线默认平坦：peak = base
                def.hp_peak = def.hp; def.mp_peak = def.mp;
                def.atk_peak = def.atk; def.def_peak = def.def;
                selected_actor_id_ = new_id;
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_SAVE " Save")) if (on_save_) on_save_();
            ImGui::Separator();

            std::vector<std::string> ids;
            for (const auto& [id, _] : db.actors) ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            std::string to_delete;
            if (ImGui::BeginTable("ActorTable", 7,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.18f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.22f);
                ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("MP", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("ATK", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("DEF", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Sprite", ImGuiTableColumnFlags_WidthStretch, 0.3f);
                ImGui::TableHeadersRow();
                for (const auto& id : ids) {
                    auto& def = db.actors[id];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable((id + "##sel_" + id).c_str(), selected_actor_id_ == id,
                            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                        selected_actor_id_ = id;
                    }
                    if (ImGui::BeginPopupContextItem() && ImGui::MenuItem("Delete")) to_delete = id;
                    ImGui::TableSetColumnIndex(1); InputTextString(("##name_" + id).c_str(), &def.name);
                    ImGui::TableSetColumnIndex(2); ImGui::InputInt(("##hp_" + id).c_str(), &def.hp);
                    ImGui::TableSetColumnIndex(3); ImGui::InputInt(("##mp_" + id).c_str(), &def.mp);
                    ImGui::TableSetColumnIndex(4); ImGui::InputInt(("##atk_" + id).c_str(), &def.atk);
                    ImGui::TableSetColumnIndex(5); ImGui::InputInt(("##def_" + id).c_str(), &def.def);
                    ImGui::TableSetColumnIndex(6); InputTextString(("##sprite_" + id).c_str(), &def.sprite);
                }
                ImGui::EndTable();
            }
            if (!to_delete.empty()) {
                db.actors.erase(to_delete);
                if (selected_actor_id_ == to_delete) selected_actor_id_.clear();
            }

            // ---- 成长曲线编辑（选中 Actor 时显示）----
            auto sel_it = db.actors.find(selected_actor_id_);
            if (sel_it != db.actors.end()) {
                auto& def = sel_it->second;
                ImGui::Separator();
                if (ImGui::TreeNode("Growth Curves", "Growth Curves (%s)", def.id.c_str())) {
                    ImGui::InputInt("Max Level", &def.max_level);
                    if (def.max_level < 1) def.max_level = 1;
                    if (def.max_level > 999) def.max_level = 999;

                    // 成长速率预设
                    static const struct { const char* name; float exp; } kPresets[] = {
                        { "Early (0.5)", 0.5f }, { "Average (1.0)", 1.0f }, { "Late (2.0)", 2.0f },
                    };
                    int preset = 3;   // 3 = custom
                    for (int i = 0; i < 3; ++i)
                        if (def.growth_exponent == kPresets[i].exp) preset = i;
                    if (ImGui::BeginCombo("Growth", preset < 3 ? kPresets[preset].name : "Custom")) {
                        for (int i = 0; i < 3; ++i) {
                            if (ImGui::Selectable(kPresets[i].name, preset == i))
                                def.growth_exponent = kPresets[i].exp;
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::InputFloat("Exponent", &def.growth_exponent, 0.05f, 0.25f, "%.2f");
                    if (def.growth_exponent < 0.05f) def.growth_exponent = 0.05f;
                    if (def.growth_exponent > 10.0f) def.growth_exponent = 10.0f;

                    // 4 参数：Lv1(base) / Max(peak)
                    struct ParamRow { const char* label; int* base; int* peak; };
                    ParamRow rows[] = {
                        { "HP",  &def.hp,  &def.hp_peak  },
                        { "MP",  &def.mp,  &def.mp_peak  },
                        { "ATK", &def.atk, &def.atk_peak },
                        { "DEF", &def.def, &def.def_peak },
                    };
                    for (auto& r : rows) {
                        ImGui::TextUnformatted(r.label);
                        ImGui::SameLine(50);
                        ImGui::PushID(r.label);
                        ImGui::SetNextItemWidth(80);
                        ImGui::InputInt("Lv1", r.base);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80);
                        ImGui::InputInt("Max", r.peak);
                        ImGui::PopID();
                    }

                    // 曲线预览
                    static const char* kParamNames[] = { "HP", "MP", "ATK", "DEF" };
                    ImGui::SetNextItemWidth(100);
                    ImGui::Combo("Preview", &curve_preview_param_, kParamNames, 4);
                    int pi = curve_preview_param_;
                    int base = pi == 0 ? def.hp : pi == 1 ? def.mp : pi == 2 ? def.atk : def.def;
                    int peak = pi == 0 ? def.hp_peak : pi == 1 ? def.mp_peak : pi == 2 ? def.atk_peak : def.def_peak;
                    int samples = def.max_level < 99 ? def.max_level : 99;
                    if (samples < 1) samples = 1;
                    float values[99];
                    for (int i = 0; i < samples; ++i) {
                        values[i] = (float)EvalStatCurve(base, peak, def.growth_exponent, i + 1, def.max_level);
                    }
                    ImGui::PlotLines("##curve", values, samples, 0, nullptr,
                        FLT_MAX, FLT_MAX, ImVec2(0, 60));
                    ImGui::TreePop();
                }
            }
            ImGui::EndTabItem();
        }

        // ---------- Skills ----------
        if (ImGui::BeginTabItem("Skills")) {
            auto& db = databases_.skill;
            if (ImGui::Button(ICON_FA_PLUS " Add Skill")) {
                std::string new_id = GenerateNextId("Skill", db.skills);
                auto& def = db.skills[new_id];
                def.id = new_id; def.name = "New Skill"; def.mp_cost = def.damage = 0; def.description = "";
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_SAVE " Save")) if (on_save_) on_save_();
            ImGui::Separator();

            std::vector<std::string> ids;
            for (const auto& [id, _] : db.skills) ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            std::string to_delete;
            if (ImGui::BeginTable("SkillTable", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableSetupColumn("MP Cost", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Damage", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableHeadersRow();
                for (const auto& id : ids) {
                    auto& def = db.skills[id];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Selectable((id + "##sel_" + id).c_str(), false,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                    if (ImGui::BeginPopupContextItem() && ImGui::MenuItem("Delete")) to_delete = id;
                    ImGui::TableSetColumnIndex(1); InputTextString(("##name_" + id).c_str(), &def.name);
                    ImGui::TableSetColumnIndex(2); ImGui::InputInt(("##mpc_" + id).c_str(), &def.mp_cost);
                    ImGui::TableSetColumnIndex(3); ImGui::InputInt(("##dmg_" + id).c_str(), &def.damage);
                    ImGui::TableSetColumnIndex(4); InputTextString(("##desc_" + id).c_str(), &def.description);
                }
                ImGui::EndTable();
            }
            if (!to_delete.empty()) db.skills.erase(to_delete);
            ImGui::EndTabItem();
        }

        // ---------- Enemies ----------
        if (ImGui::BeginTabItem("Enemies")) {
            auto& db = databases_.enemy;
            if (ImGui::Button(ICON_FA_PLUS " Add Enemy")) {
                std::string new_id = GenerateNextId("Enemy", db.enemies);
                auto& def = db.enemies[new_id];
                def.id = new_id; def.name = "New Enemy";
                def.hp = def.atk = def.def = def.gold = def.exp = 0; def.sprite = "";
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_SAVE " Save")) if (on_save_) on_save_();
            ImGui::Separator();

            std::vector<std::string> ids;
            for (const auto& [id, _] : db.enemies) ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            std::string to_delete;
            if (ImGui::BeginTable("EnemyTable", 8,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.15f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("ATK", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("DEF", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Gold", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("EXP", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Sprite", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableHeadersRow();
                for (const auto& id : ids) {
                    auto& def = db.enemies[id];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Selectable((id + "##sel_" + id).c_str(), false,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                    if (ImGui::BeginPopupContextItem() && ImGui::MenuItem("Delete")) to_delete = id;
                    ImGui::TableSetColumnIndex(1); InputTextString(("##name_" + id).c_str(), &def.name);
                    ImGui::TableSetColumnIndex(2); ImGui::InputInt(("##hp_" + id).c_str(), &def.hp);
                    ImGui::TableSetColumnIndex(3); ImGui::InputInt(("##atk_" + id).c_str(), &def.atk);
                    ImGui::TableSetColumnIndex(4); ImGui::InputInt(("##def_" + id).c_str(), &def.def);
                    ImGui::TableSetColumnIndex(5); ImGui::InputInt(("##gold_" + id).c_str(), &def.gold);
                    ImGui::TableSetColumnIndex(6); ImGui::InputInt(("##exp_" + id).c_str(), &def.exp);
                    ImGui::TableSetColumnIndex(7); InputTextString(("##sprite_" + id).c_str(), &def.sprite);
                }
                ImGui::EndTable();
            }
            if (!to_delete.empty()) db.enemies.erase(to_delete);
            ImGui::EndTabItem();
        }

        // ---------- Shops ----------
        if (ImGui::BeginTabItem("Shops")) {
            auto& db = databases_.shop;
            if (ImGui::Button(ICON_FA_PLUS " Add Shop")) {
                std::string new_id = GenerateNextId("Shop", db.shops);
                auto& def = db.shops[new_id];
                def.id = new_id;
                selected_shop_id_ = new_id;
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_SAVE " Save")) if (on_save_) on_save_();
            ImGui::Separator();

            ImGui::Columns(2, "ShopColumns", true);
            // 左栏：Shop ID 列表
            std::vector<std::string> shop_ids;
            for (const auto& [id, _] : db.shops) shop_ids.push_back(id);
            std::sort(shop_ids.begin(), shop_ids.end());
            ImGui::TextUnformatted("Shops");
            ImGui::BeginChild("ShopList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
            for (const auto& id : shop_ids) {
                bool is_selected = (selected_shop_id_ == id);
                if (ImGui::Selectable(id.c_str(), is_selected)) {
                    selected_shop_id_ = id;
                }
            }
            ImGui::EndChild();
            if (ImGui::Button("Delete Shop") && !selected_shop_id_.empty()) {
                db.shops.erase(selected_shop_id_);
                selected_shop_id_.clear();
            }

            ImGui::NextColumn();
            // 右栏：商品编辑
            auto it = db.shops.find(selected_shop_id_);
            if (it != db.shops.end()) {
                auto& shop = it->second;
                ImGui::Text("Shop: %s", shop.id.c_str());
                ImGui::Separator();

                // Item ID 下拉
                const auto& item_db = databases_.item;
                std::vector<std::string> item_ids;
                for (const auto& [id, _] : item_db.items) item_ids.push_back(id);
                std::sort(item_ids.begin(), item_ids.end());

                static std::string new_item_id;
                static int new_price = 0;
                if (new_item_id.empty() && !item_ids.empty()) new_item_id = item_ids[0];

                ImGui::Text("Item");
                ImGui::SameLine(60);
                if (ImGui::BeginCombo("##new_item", new_item_id.empty() ? "(none)" : new_item_id.c_str())) {
                    for (const auto& id : item_ids) {
                        bool selected = (new_item_id == id);
                        if (ImGui::Selectable(id.c_str(), selected)) new_item_id = id;
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Text("Price");
                ImGui::SameLine(60);
                ImGui::InputInt("##new_price", &new_price);
                ImGui::SameLine();
                if (ImGui::Button("Add Item") && !new_item_id.empty()) {
                    shop.items.push_back({new_item_id, new_price});
                }

                ImGui::Separator();
                int delete_idx = -1;
                if (ImGui::BeginTable("ShopItemTable", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Item ID", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                    ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableHeadersRow();
                    for (int i = 0; i < (int)shop.items.size(); ++i) {
                        auto& entry = shop.items[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        const auto* item_def = item_db.Find(entry.item_id);
                        std::string display = item_def ? item_def->name : entry.item_id;
                        ImGui::TextUnformatted(display.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::InputInt(("##price_" + std::to_string(i)).c_str(), &entry.price);
                        ImGui::TableSetColumnIndex(2);
                        if (ImGui::Button(("Del##" + std::to_string(i)).c_str())) delete_idx = i;
                    }
                    ImGui::EndTable();
                }
                if (delete_idx >= 0 && delete_idx < (int)shop.items.size()) {
                    shop.items.erase(shop.items.begin() + delete_idx);
                }
            } else {
                ImGui::TextDisabled("Select a shop on the left to edit items.");
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
