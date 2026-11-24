#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/LevelGen/Structure/JigsawAssembler.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class BlockManager;
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
        // fired instead of minecraft:entity_spawned
        std::string mEvent;
    };

    // a placed block the game gives a block actor; the template block keeps a banner's dye and a pot's plant
    struct BlockEntity {
        Position mPosition;
        BlockState mBlock;
        std::string mTemplateBlock;
    };

    // one village's blocks in one chunk, before shaping
    struct ChunkShare {
        std::map<Position, BlockState> mBlocks;
        std::vector<Chest> mChests;
        std::vector<Bed> mBeds;
        std::vector<BlockEntity> mBlockEntities;
        std::vector<Mob> mMobs;
    };

    static std::vector<JigsawPiece> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ);

    // null when the chunk is no village start
    static std::shared_ptr<const std::vector<JigsawPiece>> cachedPlan(const ChunkGenerator &generator, int32_t startX,
                                                                      int32_t startZ);

    // the villages starting close enough to reach the chunk, by start x then z
    static std::vector<std::shared_ptr<const std::vector<JigsawPiece>>> plansNear(const ChunkGenerator &generator,
                                                                                  int32_t chunkX, int32_t chunkZ);

    // a block of a chunk already post-processed, null for a chunk the village has not reached yet
    using Neighbours = std::function<const BlockState *(int32_t x, int32_t y, int32_t z)>;

    // features: the world the pieces go into in order, templates included; without it feature pieces are skipped
    // and draw nothing; without neighbours, the parts of a piece in other chunks read the bare terrain
    static ChunkShare placeInChunk(const ChunkGenerator &generator, const std::vector<JigsawPiece> &pieces,
                                   int32_t chunkX, int32_t chunkZ, StructureTerrain &terrain,
                                   BlockManager *features = nullptr, const Neighbours *neighbours = nullptr);

    static void addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ, TerrainBeard &beard);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk,
                      std::vector<GeneratedBlockChange> *overflow);
};
