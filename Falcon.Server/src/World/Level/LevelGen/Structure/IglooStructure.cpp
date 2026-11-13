#include "World/Level/LevelGen/Structure/IglooStructure.h"

#include "World/Actor/Mob/Hostile/ZombieVillagerV2Actor.h"
#include "World/Actor/Mob/Passive/VillagerV2Actor.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/BlockStateSet.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/ScatteredPiece.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>

namespace {
    const int32_t SHAFT_PIECE_HEIGHT = 3;
    const int32_t SHAFT_X = 2;
    const int32_t SHAFT_Z = 4;
    const int32_t BOTTOM_Z = -2;
    const int32_t BOTTOM_Y = -6;
    const int32_t SHAFT_MIN_PIECES = 4;
    const int32_t SHAFT_PIECE_RANGE = 8;
    const int8_t RED_BED_COLOR = 14;
    // the temple piece the start makes for an igloo (sub_1473C7400)
    const int32_t BOX_X = 7;
    const int32_t BOX_Y = 5;
    const int32_t BOX_Z = 8;
    const char *IGLOO_LOOT_TABLE = "loot_tables/chests/igloo_chest.json";

    struct IglooChoice {
        int32_t mQuarterTurns = 0;
        int32_t mShaftPieces = 0;
        int32_t mLootSeed = 0;
    };

    // the draws of the piece, in order, from the random the start chunk post processes with
    IglooChoice drawChoice(int64_t seed, int32_t chunkX, int32_t chunkZ) {
        std::mt19937 random = StructureFeature::createPostProcessRandom(seed, chunkX, chunkZ);

        IglooChoice choice;
        choice.mQuarterTurns = FeatureRandom::nextInt(random, 4);
        if (FeatureRandom::nextDouble(random) < 0.5)
            return choice;

        choice.mShaftPieces = FeatureRandom::nextInt(random, SHAFT_PIECE_RANGE) + SHAFT_MIN_PIECES;
        // placing a chest draws a loot seed of its own (sub_14775E430); the piece's marker then replaces it
        random();
        choice.mLootSeed = FeatureRandom::nextInt(random);
        return choice;
    }

    const BlockStateSet &airStates() {
        static const BlockStateSet states{"minecraft:air"};
        return states;
    }

    const BlockStateSet &barStates() {
        static const BlockStateSet states{"minecraft:iron_bars"};
        return states;
    }

    void addPiece(std::vector<IglooStructure::PlacedBlock> &blocks, const std::string &path, int32_t offsetX,
                  int32_t offsetY, int32_t offsetZ, int32_t quarterTurns) {
        const std::shared_ptr<const StructureTemplate> piece = StructureTemplate::load(path);
        if (piece == nullptr)
            return;

        for (const StructureTemplate::BlockEntry &entry: piece->getBlocks()) {
            IglooStructure::PlacedBlock block;
            if (!StructureBlockTable::translate(piece->getPalette()[(size_t) entry.mPaletteIndex], quarterTurns,
                                                block.mState))
                continue;

            block.x = entry.x + offsetX;
            block.y = entry.y + offsetY;
            block.z = entry.z + offsetZ;
            block.mMarker = entry.mData.getString("metadata", std::string());
            block.mBlockActor = entry.mData.getString("id", std::string());
            StructureTemplate::rotate(quarterTurns, block.x, block.z);
            blocks.push_back(block);
        }
    }

    // the files save bars unconnected and the game connects them once placed, to the terrain as well
    void connectBars(std::vector<IglooStructure::PlacedBlock> &blocks, StructureTerrain &terrain, int32_t originX,
                     int32_t originY, int32_t originZ) {
        std::map<std::tuple<int32_t, int32_t, int32_t>, BlockState> placed;
        for (const IglooStructure::PlacedBlock &block: blocks)
            placed[{block.x, block.y, block.z}] = block.mState;

        const auto connects = [&](const IglooStructure::PlacedBlock &bar, int32_t sideX, int32_t sideZ) {
            const auto found = placed.find({bar.x + sideX, bar.y, bar.z + sideZ});
            return StructureConnections::connects(
                    found != placed.end()
                            ? found->second
                            : terrain.getBlock(originX + bar.x + sideX, originY + bar.y, originZ + bar.z + sideZ),
                    sideX, sideZ);
        };

        for (IglooStructure::PlacedBlock &block: blocks) {
            if (barStates().contains(block.mState))
                block.mState = StructureConnections::connected(block.mState, connects(block, 1, 0),
                                                               connects(block, 0, -1), connects(block, 0, 1),
                                                               connects(block, -1, 0));
        }
    }

    Tag generatedMob(const char *identifier, const Vector3i &position) {
        Tag location = Tag::ofList(Tag::Type::Float);
        location.addToList(Tag::ofFloat((float) position.x + 0.5f));
        location.addToList(Tag::ofFloat((float) position.y));
        location.addToList(Tag::ofFloat((float) position.z + 0.5f));

        Tag mob = Tag::ofCompound();
        mob.putString("identifier", identifier);
        mob.put("Pos", location);
        return mob;
    }

    void placeData(Level &level, const IglooStructure::PlacedBlock &block, const Vector3i &position, int32_t lootSeed) {
        BlockActorStore &actors = level.getBlockActors();
        if (block.mBlockActor == "minecraft:chest") {
            actors.getOrCreate<ChestBlockActor>(position).setLootTable(IGLOO_LOOT_TABLE, lootSeed);
        } else if (block.mBlockActor == "minecraft:furnace") {
            actors.getOrCreate<FurnaceBlockActor>(position);
        } else if (block.mBlockActor == "minecraft:bed") {
            actors.getOrCreate<BedBlockActor>(position).setColor(RED_BED_COLOR);
        } else if (block.mMarker == "Villager") {
            // the game spawns the old villager: it turns into this one on its first tick
            level.addGeneratedEntity(generatedMob(VillagerV2Actor::IDENTIFIER, position));
        } else if (block.mMarker == "Zombie Villager") {
            level.addGeneratedEntity(generatedMob(ZombieVillagerV2Actor::IDENTIFIER, position));
        }
    }

    bool isIglooBiome(int32_t biomeId) {
        return biomeId == BiomeIds::ICE_PLAINS || biomeId == BiomeIds::COLD_TAIGA || biomeId == BiomeIds::SNOWY_SLOPES;
    }
}

std::vector<IglooStructure::PlacedBlock> IglooStructure::build(int32_t quarterTurns, int32_t shaftPieces,
                                                               StructureTerrain &terrain, int32_t originX,
                                                               int32_t originY, int32_t originZ) {
    std::vector<PlacedBlock> blocks;
    addPiece(blocks, shaftPieces > 0 ? "igloo/igloo_top_trapdoor.nbt" : "igloo/igloo_top_no_trapdoor.nbt", 0, 0, 0,
             quarterTurns);
    if (shaftPieces == 0)
        return blocks;

    for (int32_t i = 1; i <= shaftPieces; ++i)
        addPiece(blocks, "igloo/igloo_middle.nbt", SHAFT_X, -SHAFT_PIECE_HEIGHT * i, SHAFT_Z, quarterTurns);

    addPiece(blocks, "igloo/igloo_bottom.nbt", 0, BOTTOM_Y - SHAFT_PIECE_HEIGHT * shaftPieces, BOTTOM_Z, quarterTurns);
    connectBars(blocks, terrain, originX, originY, originZ);
    return blocks;
}

void IglooStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *temple = StructureFeature::get("temple");
    if (temple == nullptr)
        return;

    for (int32_t startX = chunk.getX() - 1; startX <= chunk.getX() + 1; ++startX) {
        for (int32_t startZ = chunk.getZ() - 1; startZ <= chunk.getZ() + 1; ++startZ) {
            if (!temple->isStartChunk(generator, startX, startZ))
                continue;

            const int32_t cornerX = startX * 16;
            const int32_t cornerZ = startZ * 16;
            const int32_t surface = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
            if (!isIglooBiome(generator.pickBiome(startX * 16 + 8, surface, startZ * 16 + 8)))
                continue;

            const int32_t floorY = generator.getGeneratedSolidHeight(cornerX, cornerZ) - 1;
            // IglooPiece::postProcess (sub_1473DCD60) first sets its box one under the mean ground (sub_1473CE840):
            // the box only covers the start chunk
            const bool inStart = startX == chunk.getX() && startZ == chunk.getZ();
            const int32_t boxY = inStart ? ScatteredPiece::averageGround(chunk, 0, 0, BOX_X, BOX_Z) - 1 : 0;
            const IglooChoice choice = drawChoice(generator.getSeed(), startX, startZ);
            StructureTerrain terrain(generator);
            for (const PlacedBlock &block:
                 build(choice.mQuarterTurns, choice.mShaftPieces, terrain, cornerX, floorY, cornerZ)) {
                const int32_t x = cornerX + block.x;
                const int32_t z = cornerZ + block.z;
                const int32_t y = floorY + block.y;
                if ((x >> 4) != chunk.getX() || (z >> 4) != chunk.getZ() || y < LevelChunk::MIN_Y)
                    continue;

                chunk.setBlock(x & 15, y, z & 15, block.mState);
                placeData(level, block, Vector3i(x, y, z), choice.mLootSeed);
            }
            if (inStart)
                ScatteredPiece::addBounds(level, chunk, "minecraft:igloo", 0, cornerX, boxY, cornerZ, BOX_X, BOX_Y,
                                          BOX_Z);
        }
    }
}
