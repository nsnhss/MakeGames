#include "map_settings.h"
#include "scene/world.h"
#include "scene/components/tilemap.h"
#include <algorithm>
#include <vector>

namespace mg {
namespace scene {

int ResizeAllTilemaps(World* world, int w, int h) {
    if (!world) return 0;
    w = std::max(w, 1);
    h = std::max(h, 1);

    std::vector<EntityID> entities;
    world->Query<Tilemap>(entities);
    for (auto e : entities) {
        if (auto* tm = world->GetComponent<Tilemap>(e)) {
            tm->ResizeLayers(w, h);
        }
    }
    return (int)entities.size();
}

int SyncAllTilemapTileSize(World* world, int tile_size) {
    if (!world) return 0;
    tile_size = std::max(tile_size, 1);

    std::vector<EntityID> entities;
    world->Query<Tilemap>(entities);
    for (auto e : entities) {
        if (auto* tm = world->GetComponent<Tilemap>(e)) {
            tm->tile_width = tile_size;
            tm->tile_height = tile_size;
        }
    }
    return (int)entities.size();
}

} // namespace scene
} // namespace mg
