#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <functional>

class BlockState;

/**
 * Where an end gateway leads. The searches read the level through a BlockReader, so they run on loaded chunks in the
 * game and on plain block maps in the tests. A reader returns nullptr for a block it does not know, read as air.
 */
namespace EndGatewayExit {

    using BlockReader = std::function<const BlockState *(int32_t x, int32_t y, int32_t z)>;

    // whether the terrain generated for a chunk has any block, without loading the chunk
    using TerrainCheck = std::function<bool(int32_t chunkX, int32_t chunkZ)>;

    static constexpr float EXIT_DISTANCE = 1024.0f;

    static constexpr float SEARCH_STEP = 16.0f;

    static constexpr int32_t SEARCH_STEPS = 16;

    // the height of the island the game raises when the exit has no land
    static constexpr int32_t ISLAND_Y = 75;

    static constexpr int32_t TALLEST_RADIUS = 16;

    // a new exit gateway floats this high above the tallest block around its island
    static constexpr int32_t EXIT_GATEWAY_HEIGHT = 10;

    static constexpr int32_t LANDING_RADIUS = 5;

    // the return gateway looks this many chunks around the gateway it leads back to
    static constexpr int32_t RETURN_SEARCH_CHUNKS = 3;

    static constexpr int32_t SPAWN_SEARCH_TOP_Y = 127;

    // how far from an exit the game looks for the gateway that leads back
    static constexpr int32_t RETURN_GATEWAY_RANGE = 32;

    /**
     * The point 1024 blocks out from the origin in the gateway's direction, walked back towards the origin while it
     * has terrain, then out again until it has some, 16 blocks at a time (sub_14714F400). Its y is always 0.
     */
    Vector3i findRoughExit(const Vector3i &gateway, const TerrainCheck &hasTerrain);

    /**
     * The closest column top within chunkRadius chunks of around (sub_14714FCB0). endStoneOnly keeps only end stone
     * tops, which count as valid when they are surrounded by blocks and open above: a valid top wins over any other.
     * fallback climbs the column of around itself when nothing was found. (0, 0, 0) means nothing was found.
     */
    Vector3i findSpawn(const BlockReader &reader, const Vector3i &around, bool endStoneOnly, int32_t chunkRadius,
                       bool fallback, int32_t minY, int32_t maxY);

    /**
     * The highest full solid block of the columns within radius of around, scanning down from maxY (the search of
     * sub_14716C8B0). The first column found keeps a tie. (0, 0, 0) means nothing was found.
     */
    Vector3i findTallestBlock(const BlockReader &reader, const Vector3i &around, int32_t radius, bool allowBedrock,
                              bool skipCenter, int32_t maxY);

    /** Whether an entity sent to exit would land in a block or over nothing, so the game moves it (sub_14716AEC0). */
    bool isExitBlocked(const BlockReader &reader, const Vector3i &exit);

    /** The seed of the island raised at position: the game's hash of the block position (sub_14716ACB0). */
    uint32_t islandSeed(const Vector3i &position);

    bool isZero(const Vector3i &position);

}
