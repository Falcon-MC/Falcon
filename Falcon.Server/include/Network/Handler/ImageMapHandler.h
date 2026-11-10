#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Map/ImageMap.h"

#include <cstdint>
#include <string>
#include <vector>

class Level;
class ServerNetworkHandler;
class ServerPlayer;
class World;

// image maps in the world: creation, frames on a wall, giving and live pixel updates; main thread only
class ImageMapHandler {
public:
    // one locked image map per tile in the world's map storage; the ids follow the tiles
    static std::vector<int64_t> createMaps(ServerNetworkHandler &owner, World &world,
                                           const std::vector<ImageMapTile> &tiles);

    // every position air with a solid block behind, or a frame with that facing holding nothing or a picture
    static bool canPlace(Level &level, const Vector3i &topLeft, int32_t facing, int32_t columns, int32_t rows,
                         std::string &error);

    // checks every frame first: on false nothing changed
    static bool place(ServerNetworkHandler &owner, Level &level, const Vector3i &topLeft, int32_t facing,
                      const std::vector<int64_t> &mapIds, int32_t columns, int32_t rows, bool glowing,
                      std::string &error);

    static bool give(ServerNetworkHandler &owner, ServerPlayer &player, int64_t mapId);

    // pixels holds width * height values; the changed area goes to every player who asked for the map
    static bool update(ServerNetworkHandler &owner, World &world, int64_t mapId, int32_t x, int32_t y, int32_t width,
                       int32_t height, const uint32_t *pixels, std::string &error);

    static bool isImageMap(World &world, int64_t mapId);

    // gone from memory, storage, the frames of loaded chunks and the inventories of online players; copies kept
    // elsewhere stay as blank maps
    static bool remove(ServerNetworkHandler &owner, World &world, int64_t mapId, std::string &error);
};
