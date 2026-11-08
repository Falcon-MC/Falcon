#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/LevelGen/Structure/JigsawAssembler.h"

#include <cstdint>
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
        std::string mBlock;
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
    static std::shared_ptr<const std::vector<JigsawPiece>> cachedPlan(const ChunkGenerator &generator,
                                                                      int32_t startX, int32_t startZ);

    // without features, feature pieces are skipped and draw nothing
    static ChunkShare placeInChunk(const ChunkGenerator &generator, const std::vector<JigsawPiece> &pieces,
                                   int32_t chunkX, int32_t chunkZ, StructureTerrain &terrain,
                                   BlockManager *features = nullptr);

    static void addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ, TerrainBeard &beard);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk,
                      std::vector<GeneratedBlockChange> *overflow);
};
