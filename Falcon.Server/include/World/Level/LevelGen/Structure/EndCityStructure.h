#pragma once

#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;

namespace EndCityStructure {
    struct Piece {
        // under endcity/ in the structure archive, without the extension
        std::string mTemplate;
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        int32_t mQuarterTurns = 0;
        // false: the template's air leaves the world as it is
        bool mOverwrite = true;
        int32_t mDepth = 0;
        StructurePiece::Box mBox;
    };

    // the pieces of a city standing at the block position, in the order the game adds them
    std::vector<Piece> plan(int32_t x, int32_t y, int32_t z, int32_t quarterTurns, std::mt19937 &random);

    void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
}
