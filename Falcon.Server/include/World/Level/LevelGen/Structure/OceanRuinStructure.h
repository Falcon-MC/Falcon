#pragma once

#include <cstdint>
#include <string>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;

// the pieces are drawn in the start chunk; every chunk a piece reaches finds its floor again and lays its share
class OceanRuinStructure {
public:
    struct Piece {
        std::string mTemplate;
        int32_t x = 0;
        int32_t z = 0;
        int32_t mQuarterTurns = 0;
        float mIntegrity = 1.0f;
        bool mLarge = false;
        bool mWarm = false;
    };

    // empty when the start chunk's biome has no ruins
    static std::vector<Piece> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
