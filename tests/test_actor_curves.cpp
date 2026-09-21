#include "test_framework.h"
#include "resource/actor_database.h"

using namespace mg;

MG_TEST(eval_curve_linear_midpoint) {
    // base=0 peak=100 exp=1 max=99：Lv50 = 50，两端精确
    MG_EXPECT_EQ(EvalStatCurve(0, 100, 1.0f, 1, 99), 0);
    MG_EXPECT_EQ(EvalStatCurve(0, 100, 1.0f, 50, 99), 50);
    MG_EXPECT_EQ(EvalStatCurve(0, 100, 1.0f, 99, 99), 100);
}

MG_TEST(eval_curve_early_vs_late) {
    // 早熟（0.5）中点 > 线性（1.0）中点 > 晚成（2.0）中点
    int early = EvalStatCurve(0, 100, 0.5f, 50, 99);
    int linear = EvalStatCurve(0, 100, 1.0f, 50, 99);
    int late = EvalStatCurve(0, 100, 2.0f, 50, 99);
    MG_EXPECT(early > linear);
    MG_EXPECT(linear > late);
    // 两端不受 exponent 影响
    MG_EXPECT_EQ(EvalStatCurve(0, 100, 0.5f, 1, 99), 0);
    MG_EXPECT_EQ(EvalStatCurve(0, 100, 2.0f, 99, 99), 100);
}

MG_TEST(eval_curve_clamp_level) {
    // level < 1 按 Lv1 处理；level > max 按满级处理
    MG_EXPECT_EQ(EvalStatCurve(10, 200, 1.0f, 0, 99), 10);
    MG_EXPECT_EQ(EvalStatCurve(10, 200, 1.0f, -5, 99), 10);
    MG_EXPECT_EQ(EvalStatCurve(10, 200, 1.0f, 999, 99), 200);
}

MG_TEST(eval_curve_edge_cases) {
    // max_level <= 1：恒为 base
    MG_EXPECT_EQ(EvalStatCurve(42, 99, 1.0f, 1, 1), 42);
    MG_EXPECT_EQ(EvalStatCurve(42, 99, 1.0f, 50, 1), 42);
    // peak < base：递减曲线（满级 < 起始）
    int lv1 = EvalStatCurve(100, 20, 1.0f, 1, 99);
    int lv99 = EvalStatCurve(100, 20, 1.0f, 99, 99);
    MG_EXPECT_EQ(lv1, 100);
    MG_EXPECT_EQ(lv99, 20);
    MG_EXPECT(lv99 < lv1);
    // exponent=0 / 负值被钳制，不崩不坏
    MG_EXPECT_EQ(EvalStatCurve(0, 100, 0.0f, 99, 99), 100);
    MG_EXPECT_EQ(EvalStatCurve(0, 100, -1.0f, 1, 99), 0);
}

MG_TEST(actor_curve_json_roundtrip) {
    ActorDatabase db;
    ActorDatabase::ActorDef hero;
    hero.id = "hero";
    hero.name = "Hero";
    hero.hp = 100; hero.mp = 30; hero.atk = 15; hero.def = 10;
    hero.max_level = 50;
    hero.hp_peak = 999; hero.mp_peak = 300; hero.atk_peak = 150; hero.def_peak = 120;
    hero.growth_exponent = 0.5f;
    db.actors[hero.id] = hero;

    nlohmann::json j = db.ToJson();
    auto restored = ActorDatabase::FromJson(j);
    MG_EXPECT(restored.has_value());
    const auto* h = restored->Find("hero");
    MG_EXPECT(h != nullptr);
    MG_EXPECT_EQ(h->max_level, 50);
    MG_EXPECT_EQ(h->hp_peak, 999);
    MG_EXPECT_EQ(h->mp_peak, 300);
    MG_EXPECT_EQ(h->atk_peak, 150);
    MG_EXPECT_EQ(h->def_peak, 120);
    MG_EXPECT_EQ(h->growth_exponent, 0.5f);
    // 旧字段不变
    MG_EXPECT_EQ(h->hp, 100);
}

MG_TEST(actor_curve_json_legacy_compat) {
    // 旧格式（无成长字段）：peak 回退为 base（平坦曲线），max_level=99，exponent=1
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"id", "old"}, {"name", "Old"}, {"hp", 80}, {"mp", 20},
                 {"atk", 12}, {"def", 8}, {"sprite", ""}});

    auto restored = ActorDatabase::FromJson(j);
    MG_EXPECT(restored.has_value());
    const auto* d = restored->Find("old");
    MG_EXPECT(d != nullptr);
    MG_EXPECT_EQ(d->max_level, 99);
    MG_EXPECT_EQ(d->hp_peak, 80);
    MG_EXPECT_EQ(d->mp_peak, 20);
    MG_EXPECT_EQ(d->atk_peak, 12);
    MG_EXPECT_EQ(d->def_peak, 8);
    MG_EXPECT_EQ(d->growth_exponent, 1.0f);
    // 平坦曲线任意等级恒为 base
    MG_EXPECT_EQ(EvalStatCurve(d->hp, d->hp_peak, d->growth_exponent, 50, d->max_level), 80);
}

MG_TEST(actor_battle_stats_by_level) {
    ActorDatabase::ActorDef d;
    d.id = "hero"; d.name = "Hero";
    d.hp = 100; d.mp = 20; d.atk = 10; d.def = 8;
    d.hp_peak = 999; d.mp_peak = 300; d.atk_peak = 150; d.def_peak = 120;
    d.max_level = 99; d.growth_exponent = 1.0f;

    // Lv1 = 基础值
    auto s1 = EvalActorBattleStats(d, 1);
    MG_EXPECT_EQ(s1.hp, 100);
    MG_EXPECT_EQ(s1.mp, 20);
    MG_EXPECT_EQ(s1.atk, 10);
    MG_EXPECT_EQ(s1.def, 8);

    // 满级 = 峰值
    auto s99 = EvalActorBattleStats(d, 99);
    MG_EXPECT_EQ(s99.hp, 999);
    MG_EXPECT_EQ(s99.mp, 300);
    MG_EXPECT_EQ(s99.atk, 150);
    MG_EXPECT_EQ(s99.def, 120);

    // 中间等级介于两者之间且单调
    auto s50 = EvalActorBattleStats(d, 50);
    MG_EXPECT(s50.hp > s1.hp && s50.hp < s99.hp);
    MG_EXPECT(s50.atk > s1.atk && s50.atk < s99.atk);
}

MG_TEST(actor_battle_stats_flat_curve_compat) {
    // 平坦曲线（peak=base，旧格式回退）：任意等级 = 基础值（与旧战斗行为一致）
    ActorDatabase::ActorDef d;
    d.hp = 80; d.mp = 20; d.atk = 12; d.def = 8;
    d.hp_peak = d.hp; d.mp_peak = d.mp; d.atk_peak = d.atk; d.def_peak = d.def;

    auto s = EvalActorBattleStats(d, 42);
    MG_EXPECT_EQ(s.hp, 80);
    MG_EXPECT_EQ(s.mp, 20);
    MG_EXPECT_EQ(s.atk, 12);
    MG_EXPECT_EQ(s.def, 8);
}
