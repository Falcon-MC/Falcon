#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <string>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;
class StructureTerrain;

/**
 * Igloos: the temple starts of the snowy biomes. The pieces turn around the north west corner of the start chunk
 * and may reach into the neighbouring chunks, so every chunk writes its own share of the starts around it.
 */
class IglooStructure {
public:
    struct PlacedBlock {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        BlockState mState;
        // the data of the template: the marker text of a data block, or the block actor of the block
        std::string mMarker;
        std::string mBlockActor;
    };

    // the blocks relative to the corner of the start chunk and the floor height
    static std::vector<PlacedBlock> build(int32_t quarterTurns, int32_t shaftPieces, StructureTerrain &terrain,
                                          int32_t originX, int32_t originY, int32_t originZ);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
