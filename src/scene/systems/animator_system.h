#pragma once

namespace mg {
namespace scene {

class World;

class AnimatorSystem {
public:
    void Update(float dt, World* world);
};

} // namespace scene
} // namespace mg
