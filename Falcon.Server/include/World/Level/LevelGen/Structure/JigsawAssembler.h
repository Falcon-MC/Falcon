#pragma once

#include "World/Level/LevelGen/Structure/JigsawPools.h"

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class ChunkGenerator;
class StructureTemplate;

struct JigsawBox {
    int32_t mMinX = 0;
    int32_t mMinY = 0;
    int32_t mMinZ = 0;
    int32_t mMaxX = 0;
    int32_t mMaxY = 0;
    int32_t mMaxZ = 0;
};

struct JigsawPiece {
    const JigsawElementData *mElement = nullptr;
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    int32_t mQuarterTurns = 0;
    int32_t mDepth = 0;
    JigsawBox mBox;
    // height of the ground over the box bottom, which the terrain beard meets
    int32_t mGroundLevelDelta = 1;
};

/**
 * Grows a structure from a start element. A branch is followed to its end before the next jigsaw of its parent is
 * tried, so the draws of a whole structure depend on the order pieces are accepted in.
 */
class JigsawAssembler {
public:
    JigsawAssembler(const ChunkGenerator &generator, std::mt19937 &random, int32_t maxDepth, int32_t seaLevel);

    // the start is laid on the ground at the middle of its box; empty when the element has no template
    std::vector<JigsawPiece> assemble(const JigsawElementData &start, int32_t x, int32_t y, int32_t z,
                                      int32_t quarterTurns);

private:
    struct Marker {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        // 0 down, 1 up, 2 north, 3 south, 4 west, 5 east
        int32_t mFacing = 0;
        std::string mName;
        std::string mPool;
    };

    std::vector<Marker> _markers(const JigsawElementData &element, int32_t x, int32_t y, int32_t z,
                                 int32_t quarterTurns) const;

    JigsawBox _box(const JigsawElementData &element, int32_t x, int32_t y, int32_t z, int32_t quarterTurns) const;

    // the noise terrain before surface rules and carvers, water included, held at sea level; the game keeps these
    // per chunk for the whole assembly
    int32_t _groundHeight(int32_t x, int32_t z);

    void _expand(size_t pieceIndex);

    bool _tryPool(size_t pieceIndex, const Marker &marker, const JigsawPools::Pool &pool, const int32_t target[3],
                  std::vector<JigsawBox> &inside);

    template <typename T>
    void _shuffle(std::vector<T> &values);

    const ChunkGenerator &mGenerator;
    std::mt19937 &mRandom;
    int32_t mMaxDepth;
    int32_t mSeaLevel;
    std::vector<JigsawPiece> mPieces;
    std::unordered_map<uint64_t, std::array<int32_t, 256>> mHeights;
};
