#pragma once
#include <cmath>
#include "third_party/json.hpp"

namespace mg {
namespace math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }

    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(float s) { x /= s; y /= s; return *this; }

    bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Vec2& other) const { return !(*this == other); }

    float LengthSq() const { return x * x + y * y; }
    float Length() const { return std::sqrt(LengthSq()); }

    Vec2 Normalized() const {
        double len_sq = (double)x * (double)x + (double)y * (double)y;
        double len = std::sqrt(len_sq);
        if (len > 1e-12) {
            float inv_len = (float)(1.0 / len);
            return Vec2(x * inv_len, y * inv_len);
        }
        return Vec2(0, 0);
    }

    float Dot(const Vec2& other) const { return x * other.x + y * other.y; }
};

inline void to_json(nlohmann::json& j, const Vec2& v) { j = nlohmann::json::array({v.x, v.y}); }
inline void from_json(const nlohmann::json& j, Vec2& v) {
    if (j.is_array() && j.size() >= 2) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); }
}

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

} // namespace math
} // namespace mg
