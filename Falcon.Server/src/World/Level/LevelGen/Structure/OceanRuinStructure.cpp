#include "World/Level/LevelGen/Structure/OceanRuinStructure.h"

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"
#include "World/Level/LevelGen/Structure/OceanRuinFeature.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <tuple>
#include <utility>

namespace {
    const int32_t SEA_LEVEL = 63;
    // a start's cluster reaches two chunks away at most
    const int32_t REACH = 2;
    const int32_t MONUMENT_OFFSET = 4;

    const float LARGE_INTEGRITY = 0.9f;
    const float SMALL_INTEGRITY = 0.8f;
    const float CRACKED_INTEGRITY = 0.7f;
    const float MOSSY_INTEGRITY = 0.5f;

    // the capped archaeology rule (sub_1469AFE80)
    const int32_t ARCHAEOLOGY_CAP = 5;
    const char *const WARM_LOOT_TABLE = "loot_tables/entities/warm_ocean_ruins_brushable_block.json";
    const char *const COLD_LOOT_TABLE = "loot_tables/entities/cold_ocean_ruins_brushable_block.json";

    const char *const BIG_LOOT = "loot_tables/chests/underwater_ruin_big.json";
    const char *const SMALL_LOOT = "loot_tables/chests/underwater_ruin_small.json";

    // sub_1473BFBA0: the cold small lists all open with the unfinished ruin4
    const std::array<const char *, 8> WARM_PIECES = {"ruin_warm1", "ruin_warm2", "ruin_warm3", "ruin_warm4",
                                                     "ruin_warm5", "ruin_warm6", "ruin_warm7", "ruin_warm8"};
    const std::array<const char *, 4> BIG_WARM_PIECES = {"big_ruin_warm4", "big_ruin_warm5", "big_ruin_warm6",
                                                         "big_ruin_warm7"};
    const std::array<const char *, 9> COLD_PIECES = {"ruin4", "ruin1", "ruin2", "ruin3", "ruin4",
                                                     "ruin5", "ruin6", "ruin7", "ruin8"};
    const std::array<const char *, 4> BIG_COLD_PIECES = {"big_ruin1", "big_ruin2", "big_ruin3", "big_ruin8"};

    // by quarter turns (sub_1473BE700): the corner the rotated piece grows from, and the far side of the area
    const std::array<int32_t, 4> START_X = {0, 15, 15, 0};
    const std::array<int32_t, 4> START_Z = {0, 0, 15, 15};
    const std::array<int32_t, 4> MAIN_REACH_X = {15, -15, -15, 15};
    const std::array<int32_t, 4> MAIN_REACH_Z = {15, 15, -15, -15};
    const std::array<int32_t, 4> SMALL_REACH_X = {5, -6, -5, 6};
    const std::array<int32_t, 4> SMALL_REACH_Z = {6, 5, -6, -5};

    using Position = std::tuple<int32_t, int32_t, int32_t>;

    BlockState defaultState(const char *name) {
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        return defaults != nullptr ? BlockState(name, *defaults) : BlockState(name);
    }

    BlockState waterState() {
        Tag states = Tag::ofCompound();
        states.putInt("liquid_depth", 0);
        return BlockState(BlockNames::WATER, states);
    }

    void addPieces(std::vector<OceanRuinStructure::Piece> &pieces, std::mt19937 &random, bool warm, int32_t x,
                   int32_t z, int32_t quarterTurns, bool large, float integrity) {
        OceanRuinStructure::Piece piece;
        piece.x = x;
        piece.z = z;
        piece.mQuarterTurns = quarterTurns;
        piece.mLarge = large;
        piece.mWarm = warm;
        if (warm) {
            piece.mTemplate = std::string("ruin/")
                              + (large ? BIG_WARM_PIECES[(size_t) FeatureRandom::nextInt(random, 4)]
                                       : WARM_PIECES[(size_t) FeatureRandom::nextInt(random, 8)]);
            piece.mIntegrity = integrity;
            pieces.push_back(piece);
            return;
        }

        // three finishes laid over each other, each losing more blocks
        const int32_t index = FeatureRandom::nextInt(random, large ? 4 : 9);
        const std::array<std::pair<const char *, float>, 3> finishes = {
                {{"brick", integrity}, {"cracked", CRACKED_INTEGRITY}, {"mossy", MOSSY_INTEGRITY}}};
        for (const std::pair<const char *, float> &finish: finishes) {
            const char *name = large ? BIG_COLD_PIECES[(size_t) index] : COLD_PIECES[(size_t) index];
            piece.mTemplate = std::string("ruin/") + name;
            if (large || index != 0)
                piece.mTemplate += std::string("_") + finish.first;
            piece.mIntegrity = finish.second;
            pieces.push_back(piece);
        }
    }

    // sub_1473C2B30: what the chest marker turns into, the chest itself going one block lower
    BlockState markerBlock(const OceanRuinStructure::Piece &piece) {
        const std::string &name = piece.mTemplate;
        if (name == "ruin/big_ruin1_brick" || name == "ruin/big_ruin8_brick" || name == "ruin/ruin2_brick")
            return defaultState(BlockNames::GRAVEL);
        if (name == "ruin/ruin3_brick")
            return defaultState(BlockNames::STONE_BRICKS);
        return piece.mWarm ? defaultState(BlockNames::SANDSTONE) : waterState();
    }

    // Mth::getSeed with the product cut to 32 bits before the xoroshiro seeding (sub_1477607B0)
    int64_t positionSeed(int32_t x, int32_t y, int32_t z) {
        const int64_t mixed = (116129781LL * (int64_t) z) ^ (int64_t) y ^ (int64_t) (int32_t) ((uint32_t) x * 3129871U);
        const uint64_t spread = (uint64_t) mixed * (42317861ULL * (uint64_t) mixed + 11ULL);
        return (int64_t) (int32_t) (spread >> 16);
    }

    class PiecePlacer {
    public:
        PiecePlacer(Level &level, LevelChunk &chunk, uint32_t seed, std::mt19937 &chunkRandom,
                    StructureTerrain &terrain)
                : mLevel(level), mChunk(chunk), mSeed(seed), mChunkRandom(chunkRandom), mTerrain(terrain) {}

        // sub_1473C1030 then the template piece post process (sub_14699DA70)
        void place(const OceanRuinStructure::Piece &piece) {
            const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(piece.mTemplate + ".nbt");
            if (shape == nullptr)
                return;

            int32_t farX = shape->getSizeX() - 1;
            int32_t farZ = shape->getSizeZ() - 1;
            StructureTemplate::rotate(piece.mQuarterTurns, farX, farZ);
            const int32_t minX = std::min(piece.x, piece.x + farX);
            const int32_t maxX = std::max(piece.x, piece.x + farX);
            const int32_t minZ = std::min(piece.z, piece.z + farZ);
            const int32_t maxZ = std::max(piece.z, piece.z + farZ);
            if (maxX < mChunk.getX() * 16 || minX > mChunk.getX() * 16 + 15 || maxZ < mChunk.getZ() * 16
                || minZ > mChunk.getZ() * 16 + 15)
                return;

            const int32_t originY = _floor(piece, minX, maxX, minZ, maxZ, std::abs(farX));
            const char *archaeology = piece.mWarm ? BlockNames::SAND : BlockNames::GRAVEL;
            std::vector<Position> candidates;

            // the template random: post process seed, drawn once per block the integrity is tried on
            std::mt19937 integrity(mSeed);
            for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
                const StructureTemplate::PaletteEntry &palette = shape->getPalette()[(size_t) entry.mPaletteIndex];
                // air is the ignored block; structure and jigsaw blocks are skipped by the settings
                if (palette.mName == BlockNames::AIR || palette.mName == "minecraft:structure_block"
                    || palette.mName == "minecraft:jigsaw")
                    continue;
                if (piece.mIntegrity < 1.0f
                    && (double) piece.mIntegrity < (double) integrity() * 2.3283064365386963e-10)
                    continue;

                int32_t x = entry.x;
                int32_t z = entry.z;
                StructureTemplate::rotate(piece.mQuarterTurns, x, z);
                x += piece.x;
                z += piece.z;
                const int32_t y = originY + entry.y;
                if (!_inChunk(x, z) || y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                    continue;

                BlockState state;
                if (!StructureBlockTable::translate(palette, piece.mQuarterTurns, state))
                    continue;

                if (state.mName == archaeology)
                    candidates.emplace_back(x, y, z);
                _set(x, y, z, state);
                if (entry.mData.getString("id", std::string()) == "minecraft:chest"
                    && state.mName == BlockNames::CHEST) {
                    // placing a chest draws a seed even without a loot table
                    FeatureRandom::nextInt(mChunkRandom);
                    mLevel.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(x, y, z));
                }
            }

            for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
                if (entry.mData.getString("metadata", std::string()) == "chest")
                    _chest(piece, entry, originY);
            }

            _uncover(candidates, positionSeed(piece.x, originY, piece.z), piece.mWarm);
        }

    private:
        bool _inChunk(int32_t x, int32_t z) const {
            return (x >> 4) == mChunk.getX() && (z >> 4) == mChunk.getZ();
        }

        // the neighbour chunks are read bare: the game sees them in whatever state they are
        const BlockState &_block(int32_t x, int32_t y, int32_t z) {
            return _inChunk(x, z) ? mChunk.getBlock(x & 15, y, z & 15) : mTerrain.getBlock(x, y, z);
        }

        void _set(int32_t x, int32_t y, int32_t z, const BlockState &state) {
            mChunk.setBlock(x & 15, y, z & 15, state);
            // the piece settings ask for the sea in what can hold it (sub_14775E430)
            if (y < SEA_LEVEL && LiquidPhysicsSystem::getWaterloggingLevel(state) > 0)
                mChunk.setBlock(x & 15, y, z & 15, 1, waterState());
        }

        // from the top of the water at the piece corner, through water and air down to the floor of every column;
        // a piece that sinks more than three blocks almost everywhere lies on the lowest floor
        int32_t _floor(const OceanRuinStructure::Piece &piece, int32_t minX, int32_t maxX, int32_t minZ, int32_t maxZ,
                       int32_t spanX) {
            const int32_t top = mTerrain.getTopHeight(piece.x, piece.z);
            int32_t lowest = 512;
            int32_t sunk = 0;
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    int32_t y = top - 1;
                    while (y > LevelChunk::MIN_Y + 1) {
                        const BlockState &block = _block(x, y, z);
                        BlockMaterialType material = BlockMaterialType::Air;
                        if (block.mName != BlockNames::AIR
                            && !(BlockMaterial::find(block, material) && material == BlockMaterialType::Water))
                            break;
                        --y;
                    }
                    lowest = std::min(lowest, y);
                    if (y < top - 3)
                        ++sunk;
                }
            }

            int32_t floor = top;
            if (lowest - top < -3 && sunk > spanX - 2)
                floor = lowest + 1;
            return floor - 1;
        }

        void _chest(const OceanRuinStructure::Piece &piece, const StructureTemplate::BlockEntry &entry,
                    int32_t originY) {
            int32_t x = entry.x;
            int32_t z = entry.z;
            StructureTemplate::rotate(piece.mQuarterTurns, x, z);
            x += piece.x;
            z += piece.z;
            const int32_t y = originY + entry.y;
            if (!_inChunk(x, z) || y - 1 < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                return;

            mChunk.setBlock(x & 15, y, z & 15, markerBlock(piece));
            Tag states = Tag::ofCompound();
            states.putString("minecraft:cardinal_direction", "east");
            mChunk.setBlock(x & 15, y - 1, z & 15, BlockState(BlockNames::CHEST, states));
            if (y <= SEA_LEVEL)
                mChunk.setBlock(x & 15, y - 1, z & 15, 1, waterState());

            ChestBlockActor &chest = mLevel.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(x, y - 1, z));
            chest.setLootTable(piece.mLarge ? BIG_LOOT : SMALL_LOOT, FeatureRandom::nextInt(mChunkRandom));
        }

        // the sand or gravel the piece laid here, shuffled, the first five still there made suspicious; each actor
        // takes the archaeology table and the random's next int (sub_1469B9460, loot type 3 warm, 2 cold)
        void _uncover(std::vector<Position> &candidates, int64_t seed, bool warm) {
            XoroshiroRandom random(seed);
            for (size_t i = 1; i < candidates.size(); ++i) {
                const size_t j = (size_t) random.nextInt((int32_t) i + 1);
                if (j != i)
                    std::swap(candidates[i], candidates[j]);
            }

            int32_t uncovered = 0;
            for (const Position &position: candidates) {
                const int32_t x = std::get<0>(position);
                const int32_t y = std::get<1>(position);
                const int32_t z = std::get<2>(position);
                const std::string &name = mChunk.getBlock(x & 15, y, z & 15).mName;
                const char *suspicious = name == BlockNames::SAND     ? BlockNames::SUSPICIOUS_SAND
                                         : name == BlockNames::GRAVEL ? BlockNames::SUSPICIOUS_GRAVEL
                                                                      : nullptr;
                if (suspicious == nullptr)
                    continue;

                mChunk.setBlock(x & 15, y, z & 15, defaultState(suspicious));
                BrushableBlockActor &actor = mLevel.getBlockActors().getOrCreate<BrushableBlockActor>(
                        Vector3i(x, y, z));
                actor.setType(suspicious);
                actor.setLootTable(warm ? WARM_LOOT_TABLE : COLD_LOOT_TABLE, random.nextInt());
                if (++uncovered == ARCHAEOLOGY_CAP)
                    return;
            }
        }

        Level &mLevel;
        LevelChunk &mChunk;
        uint32_t mSeed;
        std::mt19937 &mChunkRandom;
        StructureTerrain &mTerrain;
    };
}

std::vector<OceanRuinStructure::Piece> OceanRuinStructure::plan(const ChunkGenerator &generator, int32_t startX,
                                                                int32_t startZ) {
    std::vector<Piece> pieces;
    const int32_t height = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
    OceanRuinFeature::Config config;
    if (!OceanRuinFeature::getConfig(generator.pickBiome(startX * 16 + 8, height, startZ * 16 + 8), config))
        return pieces;

    // isFeatureChunk ends on the monument test of the chunk four further on both axes, which reseeds the random
    // createStart then draws from (sub_1473BE530)
    std::mt19937 random = StructureFeature::get("monument")
                                  ->createStartRandom(generator.getSeed(), startX + MONUMENT_OFFSET,
                                                      startZ + MONUMENT_OFFSET);
    const int32_t turns = FeatureRandom::nextInt(random, 4);
    const int32_t x = startX * 16 + START_X[(size_t) turns];
    const int32_t z = startZ * 16 + START_Z[(size_t) turns];
    const bool large = config.mLargeChance >= FeatureRandom::nextFloat(random);
    addPieces(pieces, random, config.mWarm, x, z, turns, large, large ? LARGE_INTEGRITY : SMALL_INTEGRITY);
    if (!large || config.mClusterChance < FeatureRandom::nextFloat(random))
        return pieces;

    const int32_t minX = std::min(x, x + MAIN_REACH_X[(size_t) turns]);
    const int32_t maxX = std::max(x, x + MAIN_REACH_X[(size_t) turns]);
    const int32_t minZ = std::min(z, z + MAIN_REACH_Z[(size_t) turns]);
    const int32_t maxZ = std::max(z, z + MAIN_REACH_Z[(size_t) turns]);

    std::vector<std::pair<int32_t, int32_t>> spots;
    const auto spot = [&](int32_t fromX, int32_t rangeX, int32_t fromZ, int32_t rangeZ) {
        const int32_t spotX = fromX + FeatureRandom::nextInt(random, rangeX);
        const int32_t spotZ = fromZ + FeatureRandom::nextInt(random, rangeZ);
        spots.emplace_back(spotX, spotZ);
    };
    spot(minX - 15, 7, minZ + 17, 6);
    spot(minX - 15, 7, minZ + 1, 6);
    spot(minX - 15, 7, minZ - 12, 4);
    spot(minX + 1, 6, minZ + 17, 6);
    spot(minX + 1, 6, minZ - 12, 2);
    spot(minX + 17, 6, minZ + 19, 5);
    spot(minX + 17, 6, minZ + 1, 6);
    spot(minX + 17, 6, minZ - 12, 4);

    const int32_t count = FeatureRandom::nextInt(random, 4) + 4;
    for (int32_t i = 0; i < count; ++i) {
        const size_t index = (size_t) FeatureRandom::nextInt(random, (int32_t) spots.size());
        const std::pair<int32_t, int32_t> chosen = spots[index];
        spots.erase(spots.begin() + (std::ptrdiff_t) index);

        const int32_t pieceTurns = FeatureRandom::nextInt(random, 4);
        const int32_t toX = chosen.first + SMALL_REACH_X[(size_t) pieceTurns];
        const int32_t toZ = chosen.second + SMALL_REACH_Z[(size_t) pieceTurns];
        // only the spots clear of the main ruin in x and z get a small one
        if (std::min(chosen.second, toZ) <= maxZ && std::max(chosen.second, toZ) >= minZ
            && std::max(chosen.first, toX) >= minX && std::min(chosen.first, toX) <= maxX)
            continue;

        addPieces(pieces, random, config.mWarm, chosen.first, chosen.second, pieceTurns, false, SMALL_INTEGRITY);
    }

    return pieces;
}

void OceanRuinStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *ruins = StructureFeature::get("ocean_ruin");
    std::vector<Piece> pieces;
    for (int32_t startX = chunk.getX() - REACH; startX <= chunk.getX() + REACH; ++startX) {
        for (int32_t startZ = chunk.getZ() - REACH; startZ <= chunk.getZ() + REACH; ++startZ) {
            if (!ruins->isStartChunk(generator, startX, startZ))
                continue;
            const std::vector<Piece> start = plan(generator, startX, startZ);
            pieces.insert(pieces.end(), start.begin(), start.end());
        }
    }
    if (pieces.empty())
        return;

    // one post process random for every ruin piece of the chunk
    const uint32_t seed = StructureFeature::getPostProcessSeed(generator.getSeed(), chunk.getX(), chunk.getZ());
    std::mt19937 chunkRandom(seed);
    StructureTerrain terrain(generator);
    PiecePlacer placer(level, chunk, seed, chunkRandom, terrain);
    for (const Piece &piece: pieces)
        placer.place(piece);
}
