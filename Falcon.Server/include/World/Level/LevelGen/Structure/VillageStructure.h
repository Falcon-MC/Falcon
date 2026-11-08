#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Structure/JigsawAssembler.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;
class StructureTerrain;
class TerrainBeard;

/**
 * Villages. The start chunk draws a town centre for the style of its biome and the jigsaw assembler grows it from
 * there, at most six pieces deep.
 */
class VillageStructure {
public:
    using Position = std::tuple<int32_t, int32_t, int32_t>;

    struct Chest {
        Position mPosition;
        std::string mLootTable;
        int32_t mLootSeed = 0;
    };

    struct Bed {
        Position mPosition;
        uint8_t mColor = 0;
    };

    struct Mob {
        Position mPosition;
        std::string mIdentifier;
    };

    // what the pieces of one village leave in one chunk, before fences, panes and stairs are shaped
    struct ChunkShare {
        std::map<Position, BlockState> mBlocks;
        std::vector<Chest> mChests;
        std::vector<Bed> mBeds;
        std::vector<Mob> mMobs;
    };

    static std::vector<JigsawPiece> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ);

    // the plan of a start, kept for the chunks the village reaches; null when the chunk is no village start
    static std::shared_ptr<const std::vector<JigsawPiece>> cachedPlan(const ChunkGenerator &generator,
                                                                      int32_t startX, int32_t startZ);

    // the blocks, chests and mobs the pieces put in a chunk, each piece as the chunk's post process places it
    static ChunkShare placeInChunk(const ChunkGenerator &generator, const std::vector<JigsawPiece> &pieces,
                                   int32_t chunkX, int32_t chunkZ, StructureTerrain &terrain);

    // adds the rigid pieces of every village near this chunk to the terrain beard
    static void addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ, TerrainBeard &beard);

    // writes the share of every village reaching this chunk: its blocks shaped against their neighbours, its chests
    // and its mobs
    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
