#include "script/script_engine.h"
#include "src/core/log.h"
#include "audio/audio_manager.h"
#include "scene/event_system.h"
#include "scene/world.h"
#include "scene/components/name.h"
#include "scene/components/transform.h"
#include <SDL.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <cstdio>
#include <cstring>
#include <cctype>
#include <vector>
#include <algorithm>
#include <string>

namespace mg {

ScriptEngine& ScriptEngine::Get() {
    static ScriptEngine instance;
    return instance;
}

ScriptEngine::~ScriptEngine() { Shutdown(); }

void ScriptEngine::Initialize() {
    if (L_) return;
    L_ = luaL_newstate();
    luaL_openlibs(L_);
    BindAPI();
}

void ScriptEngine::Shutdown() {
    if (L_) { lua_close(L_); L_ = nullptr; }
    world_ = nullptr;
    event_ = nullptr;
}

void ScriptEngine::SetContext(scene::World* world, scene::EventSystem* event) {
    world_ = world;
    event_ = event;
    BindAPI(); // 重新绑定以确保使用当前 world/event 引用
}

static std::string ToLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

// ---- C callback wrappers (static, access ScriptEngine via singleton) ----

static int l_get_int(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int def = luaL_optinteger(L, 2, 0);
    auto& e = ScriptEngine::Get();
    int v = def;
    if (e.event_) v = e.event_->GetVariableInt(key, def);
    lua_pushinteger(L, v);
    return 1;
}

static int l_set_int(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int v = (int)luaL_checkinteger(L, 2);
    auto& e = ScriptEngine::Get();
    if (e.event_) e.event_->SetVariable(key, v);
    return 0;
}

static int l_get_switch(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    auto& e = ScriptEngine::Get();
    bool v = e.event_ ? e.event_->GetSwitch(key, false) : false;
    lua_pushboolean(L, v);
    return 1;
}

static int l_set_switch(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    bool v = lua_toboolean(L, 2);
    auto& e = ScriptEngine::Get();
    if (e.event_) e.event_->SetSwitch(key, v);
    return 0;
}

static int l_find_entity(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto& e = ScriptEngine::Get();
    if (!e.world_) { lua_pushinteger(L, 0); return 1; }
    std::vector<scene::EntityID> all;
    e.world_->GetAllEntities(all);
    std::string lower = ToLower(name);
    for (auto id : all) {
        auto* nc = e.world_->GetComponent<scene::Name>(id);
        if (nc && ToLower(nc->value) == lower) {
            lua_pushinteger(L, (lua_Integer)id);
            return 1;
        }
    }
    lua_pushinteger(L, 0);
    return 1;
}

static int l_get_entity_pos(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    auto& e = ScriptEngine::Get();
    if (e.world_) {
        auto* tr = e.world_->GetComponent<scene::Transform>((scene::EntityID)entity);
        if (tr) {
            lua_newtable(L);
            lua_pushnumber(L, tr->position.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, tr->position.y); lua_setfield(L, -2, "y");
            return 1;
        }
    }
    lua_newtable(L);
    lua_pushnumber(L, 0); lua_setfield(L, -2, "x");
    lua_pushnumber(L, 0); lua_setfield(L, -2, "y");
    return 1;
}

static int l_set_entity_pos(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    auto& e = ScriptEngine::Get();
    if (e.world_) {
        auto* tr = e.world_->GetComponent<scene::Transform>((scene::EntityID)entity);
        if (tr) { tr->position.x = x; tr->position.y = y; }
    }
    return 0;
}

static int l_move_entity(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    auto& e = ScriptEngine::Get();
    if (e.world_) {
        auto* tr = e.world_->GetComponent<scene::Transform>((scene::EntityID)entity);
        if (tr) { tr->position.x += dx; tr->position.y += dy; }
    }
    return 0;
}

static int l_get_entity_scale(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    auto& e = ScriptEngine::Get();
    lua_newtable(L);
    float sx = 1.0f, sy = 1.0f;
    if (e.world_) {
        auto* tr = e.world_->GetComponent<scene::Transform>((scene::EntityID)entity);
        if (tr) { sx = tr->scale.x; sy = tr->scale.y; }
    }
    lua_pushnumber(L, sx); lua_setfield(L, -2, "x");
    lua_pushnumber(L, sy); lua_setfield(L, -2, "y");
    return 1;
}

static int l_set_entity_scale(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    float sx = (float)luaL_checknumber(L, 2);
    float sy = (float)luaL_checknumber(L, 3);
    auto& e = ScriptEngine::Get();
    if (e.world_) {
        auto* tr = e.world_->GetComponent<scene::Transform>((scene::EntityID)entity);
        if (tr) { tr->scale.x = sx; tr->scale.y = sy; }
    }
    return 0;
}

static int l_get_self_switch(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    const char* sw = luaL_checkstring(L, 2);
    auto& e = ScriptEngine::Get();
    bool v = false;
    if (e.event_ && sw && sw[0]) v = e.event_->GetSelfSwitch((scene::EntityID)entity, sw[0], false);
    lua_pushboolean(L, v);
    return 1;
}

static int l_set_self_switch(lua_State* L) {
    int entity = (int)luaL_checkinteger(L, 1);
    const char* sw = luaL_checkstring(L, 2);
    bool v = lua_toboolean(L, 3);
    auto& e = ScriptEngine::Get();
    if (e.event_ && sw && sw[0]) e.event_->SetSelfSwitch((scene::EntityID)entity, sw[0], v);
    return 0;
}

// ---- Audio 组 ----

static int l_play_se(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    AudioManager::Get().PlaySE(path, vol);
    return 0;
}

static int l_play_bgm(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    float fade = (float)luaL_optnumber(L, 3, 0.0);
    AudioManager::Get().PlayBGM(path, vol, fade);
    return 0;
}

static int l_stop_bgm(lua_State* L) {
    float fade = (float)luaL_optnumber(L, 1, 0.0);
    AudioManager::Get().StopBGM(fade);
    return 0;
}

static int l_play_bgs(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    float fade = (float)luaL_optnumber(L, 3, 0.0);
    AudioManager::Get().PlayBGS(path, vol, fade);
    return 0;
}

static int l_stop_bgs(lua_State* L) {
    float fade = (float)luaL_optnumber(L, 1, 0.0);
    AudioManager::Get().StopBGS(fade);
    return 0;
}

static int l_play_me(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    AudioManager::Get().PlayME(path, vol);
    return 0;
}

// ---- Input 组 ----

static int l_is_key_down(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    SDL_Scancode sc = SDL_GetScancodeFromName(name);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        MG_LOG_WARN("[Lua] is_key_down: unknown key name: " << name);
        lua_pushboolean(L, 0);
        return 1;
    }
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    lua_pushboolean(L, keys ? keys[sc] : 0);
    return 1;
}

static int l_get_mouse_pos(lua_State* L) {
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    lua_newtable(L);
    lua_pushinteger(L, x); lua_setfield(L, -2, "x");
    lua_pushinteger(L, y); lua_setfield(L, -2, "y");
    return 1;
}

void ScriptEngine::BindAPI() {
    if (!L_) return;
    lua_register(L_, "get_int",         l_get_int);
    lua_register(L_, "set_int",         l_set_int);
    lua_register(L_, "get_switch",      l_get_switch);
    lua_register(L_, "set_switch",      l_set_switch);
    lua_register(L_, "find_entity",     l_find_entity);
    lua_register(L_, "get_entity_pos",  l_get_entity_pos);
    lua_register(L_, "set_entity_pos",  l_set_entity_pos);
    // Entity/Transform 补充
    lua_register(L_, "move_entity",      l_move_entity);
    lua_register(L_, "get_entity_scale", l_get_entity_scale);
    lua_register(L_, "set_entity_scale", l_set_entity_scale);
    lua_register(L_, "get_self_switch",  l_get_self_switch);
    lua_register(L_, "set_self_switch",  l_set_self_switch);
    // Audio 组
    lua_register(L_, "play_se",          l_play_se);
    lua_register(L_, "play_bgm",         l_play_bgm);
    lua_register(L_, "stop_bgm",         l_stop_bgm);
    lua_register(L_, "play_bgs",         l_play_bgs);
    lua_register(L_, "stop_bgs",         l_stop_bgs);
    lua_register(L_, "play_me",          l_play_me);
    // Input 组
    lua_register(L_, "is_key_down",      l_is_key_down);
    lua_register(L_, "get_mouse_pos",    l_get_mouse_pos);
}

bool ScriptEngine::DoFile(const std::string& path) {
    if (!L_) Initialize();
    if (luaL_dofile(L_, path.c_str()) != LUA_OK) {
        MG_LOG_ERROR("[Lua] " << lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool ScriptEngine::DoString(const std::string& code) {
    if (!L_) Initialize();
    if (luaL_dostring(L_, code.c_str()) != LUA_OK) {
        MG_LOG_ERROR("[Lua] " << lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

} // namespace mg
