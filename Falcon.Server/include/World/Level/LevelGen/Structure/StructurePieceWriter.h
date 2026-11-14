#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Structure/StructurePiece.h"

#include <cstdint>
#include <random>
#include <string>

class Level;
class LevelChunk;

// writes a turned piece into one chunk, in the piece's local coordinates: only what falls inside the chunk is
// written, as the game clips a piece to the chunk box it post processes
class StructurePieceWriter {
public:
    StructurePieceWriter(Level &level, LevelChunk &chunk, const StructurePiece &piece);

    bool inChunk(int32_t x, int32_t y, int32_t z) const;

    // the chunk's block, nullptr outside the chunk box
    const BlockState *getBlock(int32_t x, int32_t y, int32_t z) const;

    void placeBlock(const BlockState &state, int32_t x, int32_t y, int32_t z);

    // the edge state on the faces, the inside state within; with skipAir only blocks already there are
    // replaced, outside the chunk reads as air
    void generateBox(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                     const BlockState &edge, const BlockState &inside, bool skipAir);

    // down through air, liquids and small plants to above the dimension floor, then grass under it turns to dirt
    void fillColumnDown(const BlockState &state, int32_t x, int32_t y, int32_t z);

    // inside the chunk and not a chest already; the direction is a chest data value, never turned with the piece,
    // and the loot seed is drawn once the chest stands
    bool createChest(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t direction,
                     const char *lootTable);

    // a monster spawner set to the mob, inside the chunk
    bool createSpawner(int32_t x, int32_t y, int32_t z, const std::string &identifier);

private:
    Level &mLevel;
    LevelChunk &mChunk;
    const StructurePiece &mPiece;
};
