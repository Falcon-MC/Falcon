#include "World/Level/LevelGen/Structure/VillageStructure.h"

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <map>
#include <string>
#include <tuple>

namespace {
    const int32_t MAX_DEPTH = 6;
    const int32_t SEA_LEVEL = 63;
    const float ABANDONED_CHANCE = 0.02f;

    // how many chunks a village can grow from its start; a town fits well inside this
    const int32_t REACH = 6;

    void rotate(int32_t quarterTurns, int32_t &x, int32_t &z) {
        for (int32_t i = 0; i < quarterTurns; ++i) {
            const int32_t oldX = x;
            x = -z;
            z = oldX;
        }
    }

    const char *styleOf(int32_t biomeId) {
        switch (biomeId) {
            case BiomeIds::DESERT:
                return "desert";
            case BiomeIds::ICE_PLAINS:
            case BiomeIds::COLD_TAIGA:
                return "snowy";
            case BiomeIds::SAVANNA:
                return "savanna";
            case BiomeIds::TAIGA:
                return "taiga";
            default:
                return "plains";
        }
    }
}

std::vector<JigsawPiece> VillageStructure::plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ) {
    const int32_t cornerX = startX * 16;
    const int32_t cornerZ = startZ * 16;
    const int32_t surface = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
    const std::string style = styleOf(generator.pickBiome(cornerX + 8, surface, cornerZ + 8));

    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr)
        return {};

    std::mt19937 random = village->createStartRandom(generator.getSeed(), startX, startZ);
    const int32_t quarterTurns = FeatureRandom::nextInt(random, 4);
    const bool abandoned = FeatureRandom::nextFloat(random) < ABANDONED_CHANCE;

    const JigsawPools::Pool *centres =
            JigsawPools::get("minecraft:village/" + std::string(abandoned ? "abandoned/" : "") + style + "/town_centers");
    if (centres == nullptr || centres->mSlots.empty())
        return {};

    const JigsawElementData *start = centres->mSlots[(size_t) FeatureRandom::nextInt(random, (int32_t) centres->mSlots.size())];
    if (start == nullptr)
        return {};

    JigsawAssembler assembler(generator, random, MAX_DEPTH, SEA_LEVEL);
    return assembler.assemble(*start, cornerX, surface, cornerZ, quarterTurns);
}

void VillageStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr)
        return;

    using Position = std::tuple<int32_t, int32_t, int32_t>;
    for (int32_t startX = chunk.getX() - REACH; startX <= chunk.getX() + REACH; ++startX) {
        for (int32_t startZ = chunk.getZ() - REACH; startZ <= chunk.getZ() + REACH; ++startZ) {
            if (!village->isStartChunk(generator, startX, startZ))
                continue;

            std::map<Position, BlockState> placed;
            std::map<Position, bool> chests;
            StructureTerrain terrain(generator);
            // rigid pieces sit at a fixed height; the ground-hugging streets project each column onto the
            // bare terrain, laying their single ground layer over whatever block tops that column
            for (const JigsawPiece &piece : plan(generator, startX, startZ)) {
                if (piece.mElement->mTemplate == nullptr)
                    continue;

                const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(piece.mElement->mTemplate);
                if (shape == nullptr)
                    continue;

                for (const StructureTemplate::BlockEntry &entry : shape->getBlocks()) {
                    const StructureTemplate::PaletteEntry &palette = shape->getPalette()[(size_t) entry.mPaletteIndex];
                    if (palette.mName == "minecraft:jigsaw" || palette.mName == "minecraft:structure_void"
                        || palette.mName == "minecraft:air")
                        continue;

                    int32_t x = entry.x;
                    int32_t z = entry.z;
                    rotate(piece.mQuarterTurns, x, z);
                    x += piece.x;
                    z += piece.z;
                    const int32_t y = piece.mElement->mTerrainMatching
                                              ? terrain.getSolidHeight(x, z) - 1 + entry.y
                                              : piece.y + entry.y;

                    BlockState state;
                    if (!StructureBlockTable::translate(palette, piece.mQuarterTurns, state))
                        continue;

                    const Position position = {x, y, z};
                    placed[position] = state;
                    if (entry.mData.getString("id", std::string()) == "minecraft:chest")
                        chests[position] = true;
                }
            }

            if (placed.empty())
                continue;

            const auto neighbour = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
                const auto found = placed.find({x, y, z});
                return found != placed.end() ? found->second : terrain.getBlock(x, y, z);
            };

            for (const auto &entry : placed) {
                const int32_t x = std::get<0>(entry.first);
                const int32_t y = std::get<1>(entry.first);
                const int32_t z = std::get<2>(entry.first);
                if ((x >> 4) != chunk.getX() || (z >> 4) != chunk.getZ() || y < LevelChunk::MIN_Y
                    || y > LevelChunk::MAX_Y)
                    continue;

                chunk.setBlock(x & 15, y, z & 15, StructureConnections::shaped(entry.second, x, y, z, neighbour));
                if (chests.find(entry.first) != chests.end())
                    level.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(x, y, z));
            }
        }
    }
}
