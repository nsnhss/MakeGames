#pragma once
#include "third_party/json.hpp"

namespace mg {
namespace math {

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    Color() = default;
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static Color White()   { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
    static Color Black()   { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
    static Color Red()     { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
    static Color Green()   { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
    static Color Blue()    { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
    static Color Yellow()  { return Color(1.0f, 1.0f, 0.0f, 1.0f); }
    static Color Cyan()    { return Color(0.0f, 1.0f, 1.0f, 1.0f); }
    static Color Magenta() { return Color(1.0f, 0.0f, 1.0f, 1.0f); }
    static Color Transparent() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const Color& other) const { return !(*this == other); }
};

inline void to_json(nlohmann::json& j, const Color& c) { j = nlohmann::json::array({c.r, c.g, c.b, c.a}); }
inline void from_json(const nlohmann::json& j, Color& c) {
    if (j.is_array()) {
        if (j.size() >= 1) c.r = j[0].get<float>();
        if (j.size() >= 2) c.g = j[1].get<float>();
        if (j.size() >= 3) c.b = j[2].get<float>();
        if (j.size() >= 4) c.a = j[3].get<float>();
    }
}

} // namespace math
} // namespace mg
