#pragma once
#include <string>

struct lua_State;

namespace mg {
namespace scene { class World; class EventSystem; }

class ScriptEngine {
public:
    static ScriptEngine& Get();
    void Initialize();
    void Shutdown();
    void SetContext(scene::World* world, scene::EventSystem* event);
    bool DoFile(const std::string& path);
    bool DoString(const std::string& code);

    // 回调函数需要访问这两个指针
    scene::World* world_ = nullptr;
    scene::EventSystem* event_ = nullptr;

private:
    ScriptEngine() = default;
    ~ScriptEngine();
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;
    void BindAPI();

    lua_State* L_ = nullptr;
};

} // namespace mg
