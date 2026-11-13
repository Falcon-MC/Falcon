#pragma once

#include "World/Level/Map/MapColors.h"

#include "Core/Math/Vector3i.h"

#include <cstdint>

class BlockState;
class MapItemSavedData;

// the world as the renderer reads it; a column whose chunk is missing is left as it was
class MapRenderSource {
public:
    virtual ~MapRenderSource() = default;

    virtual bool hasChunk(int32_t chunkX, int32_t chunkZ) = 0;

    // the y above the highest block of the column
    virtual int32_t getSurfaceY(int32_t x, int32_t z) = 0;

    virtual const BlockState &getBlock(int32_t x, int32_t y, int32_t z) = 0;

    virtual MapBiomeColors getBiome(int32_t x, int32_t y, int32_t z) = 0;

    virtual int32_t getMinY() const = 0;

    virtual int32_t getMaxY() const = 0;

    virtual bool hasCeiling() const = 0;
};

// column by column, each pixel shaded against the one north of it
namespace MapRenderer {
    void update(MapItemSavedData &data, MapRenderSource &source, const Vector3i &playerPosition);

    // the brightness steps: low, normal and high
    uint32_t shade(const float color[4], int32_t brightness);

    // the Nether ceiling pattern, true for the stone coloured pixels
    bool isNetherStone(int32_t x, int32_t z);
}
