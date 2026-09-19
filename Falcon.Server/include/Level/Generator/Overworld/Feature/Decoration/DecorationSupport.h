#pragma once

#include "Block/BlockState.h"

#include <cstdint>

class BlockManager;
class Level;
class LevelChunk;

namespace DecorationSupport {

    struct FaceOffset {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    extern const FaceOffset FACE_DOWN;
    extern const FaceOffset FACE_UP;
    extern const FaceOffset FACE_NORTH;
    extern const FaceOffset FACE_SOUTH;
    extern const FaceOffset FACE_WEST;
    extern const FaceOffset FACE_EAST;

    extern const FaceOffset ALL_FACES[6];
    extern const FaceOffset HORIZONTALS[4];
    extern const FaceOffset HORIZONTAL_PLANE[4];

    int64_t chunkHash(int32_t x, int32_t z);

    const BlockState &airState();

    const BlockState &waterState();

    bool isAir(const BlockState &state);

    bool isSolid(const BlockState &state);

    bool isTransparent(const BlockState &state);

    bool isFullBlock(const BlockState &state);

    bool isLiquid(const BlockState &state);

    bool isWater(const BlockState &state);

    bool isSand(const BlockState &state);

    bool isGrass(const BlockState &state);

    bool canBeReplaced(const BlockState &state);

    void setBlockKeepingWater(LevelChunk &chunk, int32_t x, int32_t y, int32_t z, const BlockState &state);

    int32_t getBiomeAt(Level &level, int32_t x, int32_t y, int32_t z);

    int32_t getHeightAt(Level &level, int32_t x, int32_t z);

    bool isChunkGenerated(Level &level, int32_t chunkX, int32_t chunkZ);

    bool isBiomeForest(int32_t biomeId);

    bool isBiomeBirch(int32_t biomeId);

    bool isBiomeStone(int32_t biomeId);

    bool isBiomeTaiga(int32_t biomeId);

    bool isBiomeSwamp(int32_t biomeId);

    bool isBiomeRoofed(int32_t biomeId);

    bool isBiomeMooshroomIsland(int32_t biomeId);

    bool isBiomePaleGarden(int32_t biomeId);

    bool isBiomeCherryGrove(int32_t biomeId);

    bool isBiomeBamboo(int32_t biomeId);

    bool reedsSupportValid(Level &level, const BlockState &support, int32_t x, int32_t y, int32_t z);

    BlockState withState(const BlockState &base, const char *key, int32_t value);

    BlockState withState(const BlockState &base, const char *key, const char *value);

    BlockState withByteState(const BlockState &base, const char *key, bool value);

}
