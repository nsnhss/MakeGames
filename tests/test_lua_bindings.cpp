#include "test_framework.h"
#include "script/script_engine.h"
#include "scene/world.h"
#include "scene/event_system.h"
#include "scene/components/name.h"
#include "scene/components/transform.h"

using namespace mg;
using namespace mg::scene;

// headless Lua 绑定测试：ScriptEngine 单例 + World + EventSystem（不碰 Scene/SDL 窗口）
// 注意：ScriptEngine 是进程级单例，各用例共享同一 lua_State，变量名需互不冲突。

MG_TEST(lua_variables_roundtrip) {
    World world;
    EventSystem events(&world);
    auto& se = ScriptEngine::Get();
    se.Initialize();
    se.SetContext(&world, &events);

    MG_EXPECT(se.DoString("set_int('lua_score', 42)"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_score", 0), 42);

    // Lua 侧读回 + 缺省默认值
    MG_EXPECT(se.DoString("set_int('lua_echo', get_int('lua_score', -1))"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_echo", 0), 42);
    MG_EXPECT(se.DoString("set_int('lua_def', get_int('no_such_key', 7))"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_def", 0), 7);
}

MG_TEST(lua_switch_roundtrip) {
    World world;
    EventSystem events(&world);
    auto& se = ScriptEngine::Get();
    se.SetContext(&world, &events);

    MG_EXPECT(se.DoString("set_switch('lua_door', true)"));
    MG_EXPECT(events.GetSwitch("lua_door", false));

    MG_EXPECT(se.DoString("if get_switch('lua_door') then set_switch('lua_door2', true) end"));
    MG_EXPECT(events.GetSwitch("lua_door2", false));
}

MG_TEST(lua_entity_transform_bindings) {
    World world;
    EventSystem events(&world);
    auto& se = ScriptEngine::Get();
    se.SetContext(&world, &events);

    EntityID hero = world.CreateEntity();
    world.AddComponent<Name>(hero).value = "Hero";
    world.AddComponent<Transform>(hero).position = { 10.0f, 20.0f };

    // find_entity 大小写不敏感
    MG_EXPECT(se.DoString("hero = find_entity('hero')"));
    MG_EXPECT(se.DoString("set_int('lua_found', hero ~= 0 and 1 or 0)"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_found", 0), 1);

    // get_entity_pos
    MG_EXPECT(se.DoString("p = get_entity_pos(hero); set_int('lua_px', math.floor(p.x)); set_int('lua_py', math.floor(p.y))"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_px", 0), 10);
    MG_EXPECT_EQ(events.GetVariableInt("lua_py", 0), 20);

    // set_entity_pos / move_entity
    MG_EXPECT(se.DoString("set_entity_pos(hero, 100, 200); move_entity(hero, 5, -10)"));
    auto* tr = world.GetComponent<Transform>(hero);
    MG_EXPECT_EQ(tr->position.x, 105.0f);
    MG_EXPECT_EQ(tr->position.y, 190.0f);

    // scale 读写
    MG_EXPECT(se.DoString("set_entity_scale(hero, 2.0, 3.0)"));
    MG_EXPECT_EQ(tr->scale.x, 2.0f);
    MG_EXPECT_EQ(tr->scale.y, 3.0f);
    MG_EXPECT(se.DoString("s = get_entity_scale(hero); set_int('lua_sx', math.floor(s.x))"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_sx", 0), 2);

    // 未找到实体 → 0
    MG_EXPECT(se.DoString("set_int('lua_missing', find_entity('no_such_npc'))"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_missing", -1), 0);
}

MG_TEST(lua_self_switch_bindings) {
    World world;
    EventSystem events(&world);
    auto& se = ScriptEngine::Get();
    se.SetContext(&world, &events);

    EntityID chest = world.CreateEntity();
    (void)chest;

    MG_EXPECT(se.DoString("chest = find_entity('x'); set_self_switch(5, 'A', true)"));
    MG_EXPECT(events.GetSelfSwitch(5, 'A', false));
    MG_EXPECT(!events.GetSelfSwitch(5, 'B', false));

    MG_EXPECT(se.DoString("if get_self_switch(5, 'A') then set_int('lua_ssw', 1) end"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_ssw", 0), 1);
}

MG_TEST(lua_audio_bindings_registered) {
    World world;
    EventSystem events(&world);
    auto& se = ScriptEngine::Get();
    se.SetContext(&world, &events);

    // 无声卡环境：只验证函数已注册、调用不崩（不存在的路径 → 加载失败安全返回）
    MG_EXPECT(se.DoString("play_se('nonexistent.wav')"));
    MG_EXPECT(se.DoString("play_bgm('nonexistent.wav', 0.8, 1.5)"));
    MG_EXPECT(se.DoString("play_bgs('nonexistent.wav')"));
    MG_EXPECT(se.DoString("play_me('nonexistent.wav')"));
    MG_EXPECT(se.DoString("stop_bgm(0.5); stop_bgs()"));

    // is_key_down：无效键名 → false（不断言 SDL 初始化状态）
    MG_EXPECT(se.DoString("if is_key_down('NoSuchKey123') then set_int('lua_key', 1) else set_int('lua_key', 0) end"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_key", -1), 0);
}

MG_TEST(lua_error_returns_false) {
    World world;
    EventSystem events(&world);
    auto& se = ScriptEngine::Get();
    se.SetContext(&world, &events);

    MG_EXPECT(!se.DoString("this is not valid lua !!!"));
    // 出错后状态仍可用
    MG_EXPECT(se.DoString("set_int('lua_after_err', 1)"));
    MG_EXPECT_EQ(events.GetVariableInt("lua_after_err", 0), 1);
}
