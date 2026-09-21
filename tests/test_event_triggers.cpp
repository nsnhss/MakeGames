#include "test_framework.h"
#include "scene/event_system.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/event_trigger.h"

using namespace mg::scene;
namespace math = mg::math;

// 构造一个带单页 EventTrigger 的实体：trigger 类型 + 一条 SetVariable 动作
static EntityID MakeEvent(World& world, EventPage::TriggerType trigger,
                          const char* var, int value, bool run_once = false,
                          math::Vec2 pos = {0.0f, 0.0f}) {
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e, pos);
    auto& et = world.AddComponent<EventTrigger>(e);
    EventPage page;
    page.trigger = trigger;
    page.run_once = run_once;
    Action a;
    a.cmd = ActionCmd::SetVariable;
    a.target = var;
    a.int_value = value;
    page.actions.push_back(a);
    et.pages.push_back(page);
    return e;
}

// ---- OnUpdate：每帧评估 ----

MG_TEST(event_trigger_on_update_fires_every_frame) {
    World world;
    MakeEvent(world, EventPage::TriggerType::OnUpdate, "v", 1);
    EventSystem es(&world);

    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("v"), 1);

    // 重置后下一帧应再次触发（每帧轮询语义）
    es.SetVariable("v", 0);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("v"), 1);
}

MG_TEST(event_trigger_on_update_respects_condition) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& et = world.AddComponent<EventTrigger>(e);
    EventPage page;
    page.trigger = EventPage::TriggerType::OnUpdate;
    page.condition.type = EventPage::Condition::Type::Variable;
    page.condition.name = "gold";
    page.condition.compare_op = 4; // >=
    page.condition.int_value = 10;
    Action a;
    a.cmd = ActionCmd::SetVariable;
    a.target = "gated";
    a.int_value = 1;
    page.actions.push_back(a);
    et.pages.push_back(page);

    EventSystem es(&world);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("gated"), 0); // 条件不满足

    es.SetVariable("gold", 100);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("gated"), 1); // 条件满足后触发
}

MG_TEST(event_trigger_run_once_applies_to_all_triggers) {
    World world;
    MakeEvent(world, EventPage::TriggerType::OnUpdate, "v", 1, /*run_once=*/true);
    EventSystem es(&world);

    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("v"), 1);

    // run_once 已标记 has_run，之后不再触发
    es.SetVariable("v", 0);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("v"), 0);
}

// ---- 区域触发：OnEnter / OnTouch / OnCollision / OnExit ----

MG_TEST(event_trigger_area_enter_touch_collision_exit) {
    World world;
    // 玩家（第一个带 Movement 的实体）
    EntityID player = world.CreateEntity();
    world.AddComponent<Transform>(player, math::Vec2{0.0f, 0.0f});
    world.AddComponent<Movement>(player);

    // 远处的事件实体，4 种区域触发各占一页
    EntityID ev = world.CreateEntity();
    world.AddComponent<Transform>(ev, math::Vec2{100.0f, 0.0f});
    auto& et = world.AddComponent<EventTrigger>(ev);
    auto add_page = [&et](EventPage::TriggerType t, const char* var) {
        EventPage page;
        page.trigger = t;
        Action a;
        a.cmd = ActionCmd::SetVariable;
        a.target = var;
        a.int_value = 1;
        page.actions.push_back(a);
        et.pages.push_back(page);
    };
    add_page(EventPage::TriggerType::OnEnter, "enter");
    add_page(EventPage::TriggerType::OnExit, "exit");
    add_page(EventPage::TriggerType::OnTouch, "touch");
    add_page(EventPage::TriggerType::OnCollision, "collision");

    EventSystem es(&world);

    // 第 1 帧：距离 100px > 30px，无触发
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("enter"), 0);
    MG_EXPECT_EQ(es.GetVariableInt("touch"), 0);
    MG_EXPECT_EQ(es.GetVariableInt("collision"), 0);
    MG_EXPECT_EQ(es.GetVariableInt("exit"), 0);

    // 第 2 帧：玩家移到 (90, 0)，距离 10px → 新进入：OnEnter + OnCollision + OnTouch
    world.GetComponent<Transform>(player)->position.x = 90.0f;
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("enter"), 1);
    MG_EXPECT_EQ(es.GetVariableInt("touch"), 1);
    MG_EXPECT_EQ(es.GetVariableInt("collision"), 1);
    MG_EXPECT_EQ(es.GetVariableInt("exit"), 0);

    // 第 3 帧：持续重叠 → 仅 OnTouch 重复触发，OnEnter/OnCollision 不重复
    es.SetVariable("enter", 0);
    es.SetVariable("touch", 0);
    es.SetVariable("collision", 0);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("enter"), 0);
    MG_EXPECT_EQ(es.GetVariableInt("touch"), 1);
    MG_EXPECT_EQ(es.GetVariableInt("collision"), 0);

    // 第 4 帧：玩家离开 → OnExit，OnTouch 停止
    world.GetComponent<Transform>(player)->position.x = 500.0f;
    es.SetVariable("touch", 0);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("exit"), 1);
    MG_EXPECT_EQ(es.GetVariableInt("touch"), 0);
}

MG_TEST(event_trigger_area_no_player_no_crash) {
    World world;
    MakeEvent(world, EventPage::TriggerType::OnEnter, "enter", 1, false, {0.0f, 0.0f});
    EventSystem es(&world);
    es.Update(0.016f); // 无玩家实体：不触发也不崩溃
    MG_EXPECT_EQ(es.GetVariableInt("enter"), 0);
}

// ---- OnInput：无效按键名不触发不崩溃 ----

MG_TEST(event_trigger_on_input_invalid_key_ignored) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& et = world.AddComponent<EventTrigger>(e);
    EventPage page;
    page.trigger = EventPage::TriggerType::OnInput;
    page.condition.name = "NotARealKey123"; // SDL_GetScancodeFromName 返回 UNKNOWN
    Action a;
    a.cmd = ActionCmd::SetVariable;
    a.target = "input_fired";
    a.int_value = 1;
    page.actions.push_back(a);
    et.pages.push_back(page);

    EventSystem es(&world);
    es.Update(0.016f);
    es.Update(0.016f);
    MG_EXPECT_EQ(es.GetVariableInt("input_fired"), 0);
}
