#pragma once

#include "World/Level/Chunk/ChunkStructureBounds.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;
class StructureTemplate;

// a watchtower, its overgrown copy and up to four props around it, all drawn in the start chunk
class PillagerOutpostStructure {
public:
    struct Piece {
        std::string mTemplate;
        int32_t x = 0;
        int32_t z = 0;
        int32_t mQuarterTurns = 0;
        float mIntegrity = 1.0f;
        // a prop: laid on the ground at its middle, dropped where the sea is
        bool mFeature = false;
    };

    static std::vector<Piece> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);

    // the box a piece is made with, before its post process sets it on the ground; `top`: the dimension's height
    static ChunkStructureBounds::Box startBox(const Piece &piece, const StructureTemplate &shape, int32_t top);

    // the middle, the far x and z edges, the near x and z edges
    static std::array<std::pair<int32_t, int32_t>, 5> groundPoints(const ChunkStructureBounds::Box &box);
};
