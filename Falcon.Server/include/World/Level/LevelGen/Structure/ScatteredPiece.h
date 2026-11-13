#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <random>

class Level;
class LevelChunk;

// a temple piece built by code: the start never turns it, local coordinates add to the box corner; only the part
// inside the decorated chunk is written, the way the game clips a piece to the chunk box it post processes
class ScatteredPiece {
public:
    ScatteredPiece(Level &level, LevelChunk &chunk, int32_t minX, int32_t minY, int32_t minZ);

    // above the top solid block, with water counted and leaves looked through
    static int32_t aboveTopSolid(const LevelChunk &chunk, int32_t x, int32_t z);

    // the average ground height over a box held by one chunk: the mean of
    // the ground of its columns, none counted under 0 (the dimension's slot +232)
    static int32_t averageGround(const LevelChunk &chunk, int32_t minX, int32_t minZ, int32_t sizeX, int32_t sizeZ);

    // the orientation data for stairs: the weirdo_direction branch,
    // an unturned piece swaps 2 and 3
    static BlockState stairs(const BlockState &base, int32_t direction);

    // the temple start keeps the box its piece was made with, one over the sea; the piece the
    // box its first post process moved: registered into the chunk after its post process
    static void addBounds(const Level &level, LevelChunk &chunk, const char *name, int32_t startValue, int32_t minX,
                          int32_t pieceMinY, int32_t minZ, int32_t sizeX, int32_t sizeY, int32_t sizeZ);

    int32_t worldX(int32_t x) const {
        return mMinX + x;
    }

    int32_t worldY(int32_t y) const {
        return mMinY + y;
    }

    int32_t worldZ(int32_t z) const {
        return mMinZ + z;
    }

    bool inChunk(int32_t x, int32_t y, int32_t z) const;

    // the chunk's block, nullptr outside the chunk box
    const BlockState *getBlock(int32_t x, int32_t y, int32_t z) const;

    void placeBlock(const BlockState &state, int32_t x, int32_t y, int32_t z);

    // the edge state on the faces, the inside state within;
    // with skipAir only blocks already there are replaced, outside the chunk reads as air
    void generateBox(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                     const BlockState &edge, const BlockState &inside, bool skipAir);

    // down through air, liquids and small plants, then
    // grass under the column turns to dirt
    void fillColumnDown(const BlockState &state, int32_t x, int32_t y, int32_t z);

    // inside the chunk and not a chest already; the loot seed
    // is drawn once the chest stands
    bool createChest(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t direction, const char *lootTable);

    // the facing is the data value as given
    bool createDispenser(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t facing, const char *lootTable);

private:
    Level &mLevel;
    LevelChunk &mChunk;
    int32_t mMinX;
    int32_t mMinY;
    int32_t mMinZ;
};
