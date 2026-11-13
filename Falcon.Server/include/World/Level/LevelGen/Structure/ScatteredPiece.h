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

    // LevelChunk::getAboveTopSolidBlock (sub_1438650A0, 26.60) with water counted and leaves looked through
    static int32_t aboveTopSolid(const LevelChunk &chunk, int32_t x, int32_t z);

    // ScatteredFeaturePiece::updateAverageGroundHeight (sub_1473CE840, 26.60) over a box held by one chunk: the mean of
    // the ground of its columns, none counted under 0 (the dimension's slot +232)
    static int32_t averageGround(const LevelChunk &chunk, int32_t minX, int32_t minZ, int32_t sizeX, int32_t sizeZ);

    // StructurePiece::getOrientationData (sub_147772040, 26.60) for stairs: the weirdo_direction branch
    // (sub_147771BF0), an unturned piece swaps 2 and 3
    static BlockState stairs(const BlockState &base, int32_t direction);

    // the temple start keeps the box its piece was made with, one over the sea (sub_1473C7400, 26.60); the piece the
    // box its first post process moved: registered into the chunk after its post process (sub_14CDACCA0)
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

    // StructurePiece::placeBlock (sub_147785CC0, 26.60)
    void placeBlock(const BlockState &state, int32_t x, int32_t y, int32_t z);

    // StructurePiece::generateBox (sub_147786070, 26.60): the edge state on the faces, the inside state within;
    // with skipAir only blocks already there are replaced, outside the chunk reads as air
    void generateBox(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                     const BlockState &edge, const BlockState &inside, bool skipAir);

    // StructurePiece::fillColumnDown (sub_147772AF0, 26.60): down through air, liquids and small plants, then
    // grass under the column turns to dirt
    void fillColumnDown(const BlockState &state, int32_t x, int32_t y, int32_t z);

    // StructurePiece::createChest (sub_147771D00, 26.60): inside the chunk and not a chest already; the loot seed
    // is drawn once the chest stands
    bool createChest(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t direction, const char *lootTable);

    // StructurePiece::createDispenser (sub_147772720, 26.60): the facing is the data value as given
    bool createDispenser(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t facing, const char *lootTable);

private:
    Level &mLevel;
    LevelChunk &mChunk;
    int32_t mMinX;
    int32_t mMinY;
    int32_t mMinZ;
};
