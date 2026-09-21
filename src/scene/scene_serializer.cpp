#include "scene/scene_serializer.h"
#include "scene/world.h"
#include "scene/component.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/tag.h"
#include "scene/components/sprite.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/event_trigger.h"
#include "scene/components/animator.h"
#include "scene/components/movement.h"
#include "scene/components/tilemap.h"
#include <fstream>
#include <unordered_map>
#include <functional>

using json = nlohmann::json;

namespace mg {
namespace scene {

// ---- Vec2 / Color JSON helpers ----

static json Vec2ToJson(const math::Vec2& v) {
    return json::array({v.x, v.y});
}

static math::Vec2 Vec2FromJson(const json& j) {
    if (!j.is_array() || j.size() < 2) return math::Vec2(0, 0);
    return math::Vec2(j[0].get<float>(), j[1].get<float>());
}

static json ColorToJson(const math::Color& c) {
    return json::array({c.r, c.g, c.b, c.a});
}

static math::Color ColorFromJson(const json& j) {
    if (!j.is_array() || j.size() < 4) return math::Color::White();
    return math::Color(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

// ---- Component Serializers ----

static json SerializeComponent(const Transform& comp) {
    json j;
    j["position"] = Vec2ToJson(comp.position);
    j["rotation"] = comp.rotation;
    j["scale"] = Vec2ToJson(comp.scale);
    j["sorting_order"] = comp.sorting_order;
    return j;
}

static void DeserializeTransform(World* world, EntityID entity, const json& j) {
    Transform comp;
    if (j.contains("position")) comp.position = Vec2FromJson(j["position"]);
    if (j.contains("rotation")) comp.rotation = j["rotation"].get<float>();
    if (j.contains("scale")) comp.scale = Vec2FromJson(j["scale"]);
    if (j.contains("sorting_order")) comp.sorting_order = j["sorting_order"].get<int>();
    world->AddComponent<Transform>(entity, comp);
}

static json SerializeComponent(const Name& comp) {
    json j;
    j["value"] = comp.value;
    return j;
}

static void DeserializeName(World* world, EntityID entity, const json& j) {
    Name comp;
    if (j.contains("value")) comp.value = j["value"].get<std::string>();
    world->AddComponent<Name>(entity, comp);
}

static json SerializeComponent(const Tag& comp) {
    json j;
    j["tags"] = comp.tags;
    return j;
}

static void DeserializeTag(World* world, EntityID entity, const json& j) {
    Tag comp;
    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& t : j["tags"]) {
            comp.tags.push_back(t.get<std::string>());
        }
    }
    world->AddComponent<Tag>(entity, comp);
}

static json SerializeComponent(const Sprite& comp) {
    json j;
    j["texture_path"] = comp.texture_path;
    j["pivot"] = Vec2ToJson(comp.pivot);
    j["tint"] = ColorToJson(comp.tint);
    j["flip_x"] = comp.flip_x;
    j["flip_y"] = comp.flip_y;
    j["anim_frame"] = comp.anim_frame;
    return j;
}

static void DeserializeSprite(World* world, EntityID entity, const json& j) {
    Sprite comp;
    if (j.contains("texture_path")) comp.texture_path = j["texture_path"].get<std::string>();
    if (j.contains("pivot")) comp.pivot = Vec2FromJson(j["pivot"]);
    if (j.contains("tint")) comp.tint = ColorFromJson(j["tint"]);
    if (j.contains("flip_x")) comp.flip_x = j["flip_x"].get<bool>();
    if (j.contains("flip_y")) comp.flip_y = j["flip_y"].get<bool>();
    if (j.contains("anim_frame")) comp.anim_frame = j["anim_frame"].get<int>();
    world->AddComponent<Sprite>(entity, comp);
}

static json SerializeComponent(const Camera& comp) {
    json j;
    j["projection"] = (comp.projection == Camera::Projection::Orthographic) ? "orthographic" : "perspective";
    j["ortho_size"] = comp.ortho_size;
    j["near_plane"] = comp.near_plane;
    j["far_plane"] = comp.far_plane;
    j["clear_color"] = ColorToJson(comp.clear_color);
    j["viewport"] = json::array({comp.viewport_x, comp.viewport_y, comp.viewport_w, comp.viewport_h});
    j["is_primary"] = comp.is_primary;
    j["follow_target"] = comp.follow_target;
    j["smooth_follow"] = comp.smooth_follow;
    j["follow_speed"] = comp.follow_speed;
    j["dead_zone"] = Vec2ToJson(comp.dead_zone);
    return j;
}

static void DeserializeCamera(World* world, EntityID entity, const json& j) {
    Camera comp;
    if (j.contains("projection")) {
        std::string proj = j["projection"].get<std::string>();
        comp.projection = (proj == "perspective") ? Camera::Projection::Perspective : Camera::Projection::Orthographic;
    }
    if (j.contains("ortho_size")) comp.ortho_size = j["ortho_size"].get<float>();
    if (j.contains("near_plane")) comp.near_plane = j["near_plane"].get<float>();
    if (j.contains("far_plane")) comp.far_plane = j["far_plane"].get<float>();
    if (j.contains("clear_color")) comp.clear_color = ColorFromJson(j["clear_color"]);
    if (j.contains("viewport") && j["viewport"].is_array() && j["viewport"].size() >= 4) {
        comp.viewport_x = j["viewport"][0].get<float>();
        comp.viewport_y = j["viewport"][1].get<float>();
        comp.viewport_w = j["viewport"][2].get<float>();
        comp.viewport_h = j["viewport"][3].get<float>();
    }
    if (j.contains("is_primary")) comp.is_primary = j["is_primary"].get<bool>();
    if (j.contains("follow_target")) comp.follow_target = j["follow_target"].get<EntityID>();
    if (j.contains("smooth_follow")) comp.smooth_follow = j["smooth_follow"].get<bool>();
    if (j.contains("follow_speed")) comp.follow_speed = j["follow_speed"].get<float>();
    if (j.contains("dead_zone")) comp.dead_zone = Vec2FromJson(j["dead_zone"]);
    world->AddComponent<Camera>(entity, comp);
}

static json SerializeComponent(const CameraFollowTarget&) {
    return json::object();  // marker, no data
}

static void DeserializeCameraFollowTarget(World* world, EntityID entity, const json&) {
    world->AddComponent<CameraFollowTarget>(entity);
}

static json SerializeAction(const Action& action) {
    json j;
    switch (action.cmd) {
        case ActionCmd::ShowMessage: j["cmd"] = "ShowMessage"; break;
        case ActionCmd::ShowToast: j["cmd"] = "ShowToast"; break;
        case ActionCmd::SetVariable: j["cmd"] = "SetVariable"; break;
        case ActionCmd::SetSelfSwitch: j["cmd"] = "SetSelfSwitch"; break;
        case ActionCmd::Wait: j["cmd"] = "Wait"; break;
        case ActionCmd::TransferPlayer: j["cmd"] = "TransferPlayer"; break;
        case ActionCmd::PlayAnimation: j["cmd"] = "PlayAnimation"; break;
        case ActionCmd::PlaySound: j["cmd"] = "PlaySound"; break;
        case ActionCmd::PlayBGM: j["cmd"] = "PlayBGM"; break;
        case ActionCmd::StopBGM: j["cmd"] = "StopBGM"; break;
        case ActionCmd::PlayBGS: j["cmd"] = "PlayBGS"; break;
        case ActionCmd::StopBGS: j["cmd"] = "StopBGS"; break;
        case ActionCmd::PlayME: j["cmd"] = "PlayME"; break;
        case ActionCmd::AddItem: j["cmd"] = "AddItem"; break;
        case ActionCmd::RemoveItem: j["cmd"] = "RemoveItem"; break;
        case ActionCmd::ShopProcessing: j["cmd"] = "ShopProcessing"; break;
        case ActionCmd::BattleProcessing: j["cmd"] = "BattleProcessing"; break;
        case ActionCmd::ShowUI: j["cmd"] = "ShowUI"; break;
        case ActionCmd::HideUI: j["cmd"] = "HideUI"; break;
        case ActionCmd::If: j["cmd"] = "If"; break;
        case ActionCmd::Else: j["cmd"] = "Else"; break;
        case ActionCmd::EndIf: j["cmd"] = "EndIf"; break;
        case ActionCmd::Loop: j["cmd"] = "Loop"; break;
        case ActionCmd::BreakLoop: j["cmd"] = "BreakLoop"; break;
        case ActionCmd::CallScript: j["cmd"] = "CallScript"; break;
        default: j["cmd"] = "Unknown"; break;
    }
    if (!action.text.empty()) j["text"] = action.text;
    if (!action.target.empty()) j["target"] = action.target;
    if (!action.param.empty()) j["param"] = action.param;
    if (action.int_value != 0) j["int_value"] = action.int_value;
    if (action.float_value != 0.0f) j["float_value"] = action.float_value;
    if (action.bool_value) j["bool_value"] = action.bool_value;
    return j;
}

static Action DeserializeAction(const json& j) {
    Action action;
    if (!j.contains("cmd")) return action;
    std::string cmd = j["cmd"].get<std::string>();
    if (cmd == "ShowMessage") action.cmd = ActionCmd::ShowMessage;
    else if (cmd == "ShowToast") action.cmd = ActionCmd::ShowToast;
    else if (cmd == "SetVariable") action.cmd = ActionCmd::SetVariable;
    else if (cmd == "SetSelfSwitch") action.cmd = ActionCmd::SetSelfSwitch;
    else if (cmd == "Wait") action.cmd = ActionCmd::Wait;
    else if (cmd == "TransferPlayer") action.cmd = ActionCmd::TransferPlayer;
    else if (cmd == "PlayAnimation") action.cmd = ActionCmd::PlayAnimation;
    else if (cmd == "PlaySound") action.cmd = ActionCmd::PlaySound;
    else if (cmd == "PlayBGM") action.cmd = ActionCmd::PlayBGM;
    else if (cmd == "StopBGM") action.cmd = ActionCmd::StopBGM;
    else if (cmd == "PlayBGS") action.cmd = ActionCmd::PlayBGS;
    else if (cmd == "StopBGS") action.cmd = ActionCmd::StopBGS;
    else if (cmd == "PlayME") action.cmd = ActionCmd::PlayME;
    else if (cmd == "AddItem") action.cmd = ActionCmd::AddItem;
    else if (cmd == "RemoveItem") action.cmd = ActionCmd::RemoveItem;
    else if (cmd == "ShopProcessing") action.cmd = ActionCmd::ShopProcessing;
    else if (cmd == "BattleProcessing") action.cmd = ActionCmd::BattleProcessing;
    else if (cmd == "ShowUI") action.cmd = ActionCmd::ShowUI;
    else if (cmd == "HideUI") action.cmd = ActionCmd::HideUI;
    else if (cmd == "If") action.cmd = ActionCmd::If;
    else if (cmd == "Else") action.cmd = ActionCmd::Else;
    else if (cmd == "EndIf") action.cmd = ActionCmd::EndIf;
    else if (cmd == "Loop") action.cmd = ActionCmd::Loop;
    else if (cmd == "BreakLoop") action.cmd = ActionCmd::BreakLoop;
    else if (cmd == "CallScript") action.cmd = ActionCmd::CallScript;

    if (j.contains("text")) action.text = j["text"].get<std::string>();
    if (j.contains("target")) action.target = j["target"].get<std::string>();
    if (j.contains("param")) action.param = j["param"].get<std::string>();
    if (j.contains("int_value")) action.int_value = j["int_value"].get<int>();
    if (j.contains("float_value")) action.float_value = j["float_value"].get<float>();
    if (j.contains("bool_value")) action.bool_value = j["bool_value"].get<bool>();
    return action;
}

static std::string TriggerTypeToString(EventPage::TriggerType type) {
    switch (type) {
        case EventPage::TriggerType::OnStart: return "OnStart";
        case EventPage::TriggerType::OnInteract: return "OnInteract";
        case EventPage::TriggerType::OnEnter: return "OnEnter";
        case EventPage::TriggerType::OnExit: return "OnExit";
        case EventPage::TriggerType::OnTouch: return "OnTouch";
        case EventPage::TriggerType::OnUpdate: return "OnUpdate";
        case EventPage::TriggerType::OnVariableChange: return "OnVariableChange";
        case EventPage::TriggerType::OnInput: return "OnInput";
        case EventPage::TriggerType::OnCollision: return "OnCollision";
    }
    return "Unknown";
}

static EventPage::TriggerType TriggerTypeFromString(const std::string& s) {
    if (s == "OnStart") return EventPage::TriggerType::OnStart;
    if (s == "OnInteract") return EventPage::TriggerType::OnInteract;
    if (s == "OnEnter") return EventPage::TriggerType::OnEnter;
    if (s == "OnExit") return EventPage::TriggerType::OnExit;
    if (s == "OnTouch") return EventPage::TriggerType::OnTouch;
    if (s == "OnUpdate") return EventPage::TriggerType::OnUpdate;
    if (s == "OnVariableChange") return EventPage::TriggerType::OnVariableChange;
    if (s == "OnInput") return EventPage::TriggerType::OnInput;
    if (s == "OnCollision") return EventPage::TriggerType::OnCollision;
    return EventPage::TriggerType::OnInteract;
}

static json SerializeComponent(const EventTrigger& comp) {
    json j;
    json pages = json::array();
    for (const auto& page : comp.pages) {
        json pj;
        pj["trigger"] = TriggerTypeToString(page.trigger);
        pj["run_once"] = page.run_once;

        json cond;
        switch (page.condition.type) {
            case EventPage::Condition::Type::None: cond["type"] = "None"; break;
            case EventPage::Condition::Type::Variable: cond["type"] = "Variable"; break;
            case EventPage::Condition::Type::Switch: cond["type"] = "Switch"; break;
            case EventPage::Condition::Type::SelfSwitch: cond["type"] = "SelfSwitch"; break;
        }
        if (!page.condition.name.empty()) cond["name"] = page.condition.name;
        cond["compare_op"] = page.condition.compare_op;
        cond["int_value"] = page.condition.int_value;
        cond["bool_value"] = page.condition.bool_value;
        pj["condition"] = cond;

        json actions = json::array();
        for (const auto& action : page.actions) {
            actions.push_back(SerializeAction(action));
        }
        pj["actions"] = actions;
        pages.push_back(pj);
    }
    j["pages"] = pages;
    j["self_switch_a"] = comp.self_switch_a;
    j["self_switch_b"] = comp.self_switch_b;
    j["self_switch_c"] = comp.self_switch_c;
    j["self_switch_d"] = comp.self_switch_d;
    return j;
}

static json SerializeComponent(const Animator& comp) {
    json j;
    j["frame_count"] = comp.frame_count;
    j["columns"] = comp.columns;
    j["fps"] = comp.fps;
    j["playing"] = comp.playing;
    return j;
}

static json SerializeComponent(const Movement& comp) {
    json j;
    j["velocity"] = Vec2ToJson(comp.velocity);
    j["speed"] = comp.speed;
    j["mode"] = static_cast<int>(comp.mode);
    return j;
}

static void DeserializeMovement(World* world, EntityID entity, const json& j) {
    Movement comp;
    if (j.contains("velocity")) comp.velocity = Vec2FromJson(j["velocity"]);
    if (j.contains("speed")) comp.speed = j["speed"].get<float>();
    if (j.contains("mode")) comp.mode = static_cast<Movement::Mode>(j["mode"].get<int>());
    world->AddComponent<Movement>(entity, comp);
}

static json SerializeComponent(const Tilemap& comp) {
    return comp;
}

static void DeserializeTilemap(World* world, EntityID entity, const json& j) {
    Tilemap comp = j.get<Tilemap>();
    world->AddComponent<Tilemap>(entity, comp);
}

static void DeserializeAnimator(World* world, EntityID entity, const json& j) {
    Animator comp;
    if (j.contains("frame_count")) comp.frame_count = j["frame_count"].get<int>();
    if (j.contains("columns")) comp.columns = j["columns"].get<int>();
    if (j.contains("fps")) comp.fps = j["fps"].get<float>();
    if (j.contains("playing")) comp.playing = j["playing"].get<bool>();
    world->AddComponent<Animator>(entity, comp);
}

static void DeserializeEventTrigger(World* world, EntityID entity, const json& j) {
    EventTrigger comp;
    if (j.contains("pages") && j["pages"].is_array()) {
        for (const auto& pj : j["pages"]) {
            EventPage page;
            if (pj.contains("trigger")) page.trigger = TriggerTypeFromString(pj["trigger"].get<std::string>());
            if (pj.contains("run_once")) page.run_once = pj["run_once"].get<bool>();
            if (pj.contains("condition")) {
                const auto& cj = pj["condition"];
                std::string ctype = cj.value("type", "None");
                if (ctype == "Variable") page.condition.type = EventPage::Condition::Type::Variable;
                else if (ctype == "Switch") page.condition.type = EventPage::Condition::Type::Switch;
                else if (ctype == "SelfSwitch") page.condition.type = EventPage::Condition::Type::SelfSwitch;
                else page.condition.type = EventPage::Condition::Type::None;
                page.condition.name = cj.value("name", "");
                page.condition.compare_op = cj.value("compare_op", 0);
                page.condition.int_value = cj.value("int_value", 0);
                page.condition.bool_value = cj.value("bool_value", false);
            }
            if (pj.contains("actions") && pj["actions"].is_array()) {
                for (const auto& aj : pj["actions"]) {
                    page.actions.push_back(DeserializeAction(aj));
                }
            }
            comp.pages.push_back(page);
        }
    }
    if (j.contains("self_switch_a")) comp.self_switch_a = j["self_switch_a"].get<bool>();
    if (j.contains("self_switch_b")) comp.self_switch_b = j["self_switch_b"].get<bool>();
    if (j.contains("self_switch_c")) comp.self_switch_c = j["self_switch_c"].get<bool>();
    if (j.contains("self_switch_d")) comp.self_switch_d = j["self_switch_d"].get<bool>();
    world->AddComponent<EventTrigger>(entity, comp);
}

// ---- Scene Serializer ----

json SceneSerializer::Serialize(Scene* scene) {
    if (!scene) return json{};

    json j;
    j["name"] = scene->GetName();
    j["width"] = scene->GetWidth();
    j["height"] = scene->GetHeight();
    j["tile_size"] = scene->GetTileSize();
    j["mode"] = (scene->GetMode() == Scene::Mode::TopDown) ? "topdown" : "sidescroller";
    j["bgm"] = scene->GetBGMPath();
    auto bgc = scene->GetBackgroundColor();
    j["background_color"] = {bgc.r, bgc.g, bgc.b, bgc.a};

    json entities = json::array();
    auto* world = scene->GetWorld();
    if (world) {
        std::vector<EntityID> entity_list;
        world->GetAllEntities(entity_list);

        for (EntityID e : entity_list) {
            json ej;
            ej["id"] = e;
            json components = json::object();

            if (auto* comp = world->GetComponent<Transform>(e)) {
                components["Transform"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Name>(e)) {
                components["Name"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Tag>(e)) {
                components["Tag"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Sprite>(e)) {
                components["Sprite"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Camera>(e)) {
                components["Camera"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<CameraFollowTarget>(e)) {
                components["CameraFollowTarget"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<EventTrigger>(e)) {
                components["EventTrigger"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Animator>(e)) {
                components["Animator"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Movement>(e)) {
                components["Movement"] = SerializeComponent(*comp);
            }
            if (auto* comp = world->GetComponent<Tilemap>(e)) {
                components["Tilemap"] = SerializeComponent(*comp);
            }

            ej["components"] = components;
            entities.push_back(ej);
        }
    }
    j["entities"] = entities;
    return j;
}

std::unique_ptr<Scene> SceneSerializer::Deserialize(const json& j) {
    auto scene = std::make_unique<Scene>();
    scene->Initialize();

    if (j.contains("name")) scene->SetName(j["name"].get<std::string>());
    if (j.contains("width")) scene->SetSize(j["width"].get<int>(), scene->GetHeight());
    if (j.contains("height")) scene->SetSize(scene->GetWidth(), j["height"].get<int>());
    if (j.contains("tile_size")) scene->SetTileSize(j["tile_size"].get<int>());
    if (j.contains("mode")) {
        std::string mode = j["mode"].get<std::string>();
        scene->SetMode((mode == "sidescroller") ? Scene::Mode::SideScroller : Scene::Mode::TopDown);
    }
    if (j.contains("bgm")) scene->SetBGMPath(j["bgm"].get<std::string>());
    if (j.contains("background_color") && j["background_color"].is_array() && j["background_color"].size() >= 3) {
        auto& c = j["background_color"];
        scene->SetBackgroundColor(math::Color(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c.size() >= 4 ? c[3].get<float>() : 1.0f));
    }

    auto* world = scene->GetWorld();
    if (j.contains("entities") && j["entities"].is_array()) {
        for (const auto& ej : j["entities"]) {
            if (!ej.contains("id")) continue;
            EntityID id = ej["id"].get<EntityID>();

            // 确保 entity 存在（如果 id 大于当前 next_entity_id，需要先创建占位）
            // 简单处理：直接 CreateEntity，但这样 id 可能不匹配
            // 更好的方式：创建一个指定 id 的 entity
            EntityID entity = world->CreateEntity();
            (void)id; // 序列化时忽略原 id，使用新分配的 id

            if (ej.contains("components") && ej["components"].is_object()) {
                const auto& comps = ej["components"];
                if (comps.contains("Transform")) DeserializeTransform(world, entity, comps["Transform"]);
                if (comps.contains("Name")) DeserializeName(world, entity, comps["Name"]);
                if (comps.contains("Tag")) DeserializeTag(world, entity, comps["Tag"]);
                if (comps.contains("Sprite")) DeserializeSprite(world, entity, comps["Sprite"]);
                if (comps.contains("Camera")) DeserializeCamera(world, entity, comps["Camera"]);
                if (comps.contains("CameraFollowTarget")) DeserializeCameraFollowTarget(world, entity, comps["CameraFollowTarget"]);
                if (comps.contains("EventTrigger")) DeserializeEventTrigger(world, entity, comps["EventTrigger"]);
                if (comps.contains("Animator")) DeserializeAnimator(world, entity, comps["Animator"]);
                if (comps.contains("Movement")) DeserializeMovement(world, entity, comps["Movement"]);
                if (comps.contains("Tilemap")) DeserializeTilemap(world, entity, comps["Tilemap"]);
            }
        }
    }

    return scene;
}

bool SceneSerializer::SaveToFile(Scene* scene, const std::string& path) {
    try {
        json j = Serialize(scene);
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

std::unique_ptr<Scene> SceneSerializer::LoadFromFile(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return nullptr;
        json j;
        f >> j;
        return Deserialize(j);
    } catch (...) {
        return nullptr;
    }
}

} // namespace scene
} // namespace mg
