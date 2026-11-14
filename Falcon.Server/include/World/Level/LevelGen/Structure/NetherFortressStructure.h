#pragma once

#include "World/Level/LevelGen/Structure/NetherFortressPieces.h"

#include <cstdint>
#include <memory>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;

// the fortresses reaching a chunk: planned once in their start chunk, then laid a chunk at a time
class NetherFortressStructure {
public:
    struct Plan {
        int32_t mStartX = 0;
        int32_t mStartZ = 0;
        std::vector<NetherFortressPieces::Piece> mPieces;
        // the box around every piece
        StructurePiece::Box mBox;
    };

    // the nether sea level the fortress height is drawn above
    static const int32_t SEA_LEVEL;

    // a start's pieces stay within this many chunks of it
    static const int32_t REACH;

    // nullptr when the chunk starts no fortress
    static std::shared_ptr<const Plan> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ);

    // the fortresses whose box meets the chunk, by start x then z
    static std::vector<std::shared_ptr<const Plan>> plansNear(const ChunkGenerator &generator, int32_t chunkX,
                                                              int32_t chunkZ);

    // lays the pieces of every fortress reaching the chunk and records them in its structure bounds
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);

    // one piece's share of the chunk; the random is the chunk's post process random
    static void build(const NetherFortressPieces::Piece &piece, Level &level, LevelChunk &chunk,
                      std::mt19937 &random);

    static const char *const LOOT_TABLE;
};
