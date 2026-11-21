#include "World/Level/LevelGen/Nether/NetherStructures.h"

#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Nether/NetherGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/JigsawPlacer.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <map>
#include <mutex>
#include <tuple>
#include <utility>

namespace {
    class NetherStart : public StructureFeature {
    public:
        NetherStart(const char *name, int32_t spacing, int32_t separation, int32_t salt)
                : StructureFeature(name, spacing, separation, salt, Spread::Linear) {}

        DimensionType getDimension() const override {
            return DimensionType::Nether;
        }
    };

    class BastionStart : public NetherStart {
    public:
        BastionStart() : NetherStart("bastion_remnant", 30, 4, 30084232) {}

    private:
        bool _isValidStart(const ChunkGenerator &generator, int32_t x, int32_t z) const override {
            if (generator.pickBiome(x * 16, 1, z * 16) == NetherBiomeIds::BASALT_DELTAS)
                return false;

            std::mt19937 random = createStartRandom(generator.getSeed(), x, z);
            return FeatureRandom::nextInt(random, 6) >= 2;
        }
    };

    class PortalStart : public NetherStart {
    public:
        PortalStart() : NetherStart("ruined_portal_nether", 25, 10, 40552231) {}

    private:
        bool _isValidStart(const ChunkGenerator &, int32_t, int32_t) const override {
            return true;
        }
    };

    const BastionStart BASTION;
    const PortalStart PORTAL;

    using Box = ChunkStructureBounds::Box;

    struct BastionPoolData {
        const char *mName;
        struct Slot {
            const char *mLocation;
            int32_t mWeight;
            const char *mProcessors;
        };
        std::vector<Slot> mSlots;
    };

    struct BastionPools {
        std::map<std::string, JigsawPlacer::Element> mElements;
        std::map<std::string, JigsawPlacer::Pool> mPools;

        BastionPools() {
            mPools.try_emplace("minecraft:empty");
            const BastionPoolData data[] = {
#include "BastionPools.inc"
            };
            for (const BastionPoolData &row: data) {
                auto &pool = mPools[row.mName];
                pool.mFallback = "minecraft:empty";
                for (const auto &[path, weight, processors]: row.mSlots) {
                    auto &element = mElements[path];
                    element.mLocation = path;
                    element.mProcessors = processors;
                    pool.mSlots.insert(pool.mSlots.end(), weight, &element);
                }
            }
        }
    };

    std::vector<JigsawPlacer::Piece> bastionPlan(const ChunkGenerator &generator, const StructureChunkPos &start) {
        static const BastionPools pools;
        static constexpr const char *FAMILIES[] = {"minecraft:bastion/bridge/start",
                                                   "minecraft:bastion/treasure/starters",
                                                   "minecraft:bastion/hoglin_stable/origin",
                                                   "minecraft:bastion/units/air_base"};
        std::mt19937 random = BASTION.createStartRandom(generator.getSeed(), start.x, start.z);
        FeatureRandom::nextInt(random, 6);
        const int32_t rotation = FeatureRandom::nextInt(random, 4);
        const char *family = FAMILIES[FeatureRandom::nextInt(random, 4)];
        bool first = true;
        JigsawPlacer placer(
                [&](int32_t bound) {
                    if (std::exchange(first, false))
                        return rotation;
                    return FeatureRandom::nextInt(random, bound);
                },
                [](const std::string &name) -> const JigsawPlacer::Pool * {
                    const auto found = pools.mPools.find(name);
                    return found == pools.mPools.end() ? nullptr : &found->second;
                },
                [](const JigsawPlacer::Element &element) {
                    const auto shape = StructureTemplate::load(element.mLocation);
                    return shape != nullptr ? JigsawPlacer::templateOf(*shape) : nullptr;
                },
                [](int32_t, int32_t, JigsawPlacer::Projection) { return 0; });

        constexpr int32_t SEA_LEVEL = NetherGeneratorConstants::LAVA_LEVEL + 1;
        JigsawPlacer::Settings settings;
        settings.mStartPool = family;
        settings.mStartX = start.x * 16;
        settings.mStartZ = start.z * 16;
        // the placement target is one above sea level before the ground delta
        settings.mStartY = SEA_LEVEL + 1;
        // collision bounds are centred before translating the root above the lava
        settings.mAssemblyOriginY = 0;
        settings.mForwardShuffle = true;
        settings.mMaxDepth = 15;
        settings.mMaxDistanceHorizontal = 80;
        settings.mMaxDistanceVertical = 80;
        settings.mDimensionHeight = Dimension::getMaxY(DimensionType::Nether) + 1;
        JigsawPlacer::Result result;
        if (!placer.place(settings, result))
            return {};
        return std::move(result.mPieces);
    }

    std::shared_ptr<const std::vector<JigsawPlacer::Piece>> cachedBastionPlan(const ChunkGenerator &generator,
                                                                          const StructureChunkPos &start) {
        static std::mutex mutex;
        static std::map<std::tuple<int64_t, int32_t, int32_t>,
                        std::shared_ptr<const std::vector<JigsawPlacer::Piece>>> plans;
        const auto key = std::tuple(generator.getSeed(), start.x, start.z);
        {
            const std::lock_guard lock(mutex);
            const auto found = plans.find(key);
            if (found != plans.end())
                return found->second;
        }

        const auto plan = std::make_shared<const std::vector<JigsawPlacer::Piece>>(bastionPlan(generator, start));
        const std::lock_guard lock(mutex);
        if (plans.size() >= 64)
            plans.clear();
        return plans.try_emplace(key, plan).first->second;
    }

    void placeBastion(const std::vector<JigsawPlacer::Piece> &plan, Level &level,
                      LevelChunk &chunk, const Box &clip, uint32_t seed) {
        std::mt19937 lootRandom(seed);
        for (const JigsawPlacer::Piece &piece: plan) {
            const auto &bounds = piece.mBox;
            const Box box{bounds.mMinX, bounds.mMinY, bounds.mMinZ, bounds.mMaxX, bounds.mMaxY, bounds.mMaxZ};
            if (!box.intersects(clip))
                continue;

            const auto shape = StructureTemplate::load(piece.mElement->mLocation);
            if (shape == nullptr)
                continue;

            std::mt19937 random(seed);
            const auto &processors = StructureProcessors::get(piece.mElement->mProcessors);
            std::vector<const StructureProcessors::Rule *> rules;
            rules.reserve(processors.size());
            for (const auto &rule: processors)
                rules.push_back(&rule);

            for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
                int32_t dx = entry.x;
                int32_t dz = entry.z;
                StructureTemplate::rotate(piece.mQuarterTurns, dx, dz);
                const int32_t x = piece.x + dx;
                const int32_t y = piece.y + entry.y;
                const int32_t z = piece.z + dz;
                if (!clip.contains(x, y, z))
                    continue;

                const auto &palette = shape->getPalette()[entry.mPaletteIndex];
                BlockState block;
                const bool jigsaw = palette.mName == "minecraft:jigsaw";
                if (jigsaw) {
                    const auto finalState = entry.mData.getString("final_state", BlockNames::AIR);
                    if (finalState == "minecraft:structure_void"
                        || !StructureBlockTable::translateFinalState(finalState, piece.mQuarterTurns, block))
                        continue;
                } else if (palette.mName == "minecraft:structure_block" || palette.mName == "minecraft:structure_void"
                           || !StructureBlockTable::translate(palette, piece.mQuarterTurns, block)) {
                    continue;
                }

                const int32_t localX = x - clip.minX;
                const int32_t localZ = z - clip.minZ;
                if (!rules.empty()) {
                    const BlockState &world = chunk.getBlock(localX, y, localZ);
                    if (jigsaw) {
                        std::mt19937 connectorRandom(seed);
                        block = StructureProcessors::apply(rules, block, world, connectorRandom, entry.y);
                    } else {
                        block = StructureProcessors::apply(rules, block, world, random, entry.y);
                    }
                }
                chunk.setBlock(localX, y, localZ, block);
                if (block.mName != BlockNames::CHEST)
                    continue;

                const std::string table = entry.mData.getString("LootTable");
                if (table.empty())
                    continue;
                const size_t colon = table.find(':');
                const std::string path = "loot_tables/" + table.substr(colon == std::string::npos ? 0 : colon + 1)
                                         + ".json";
                const int32_t lootSeed = entry.mData.contains("LootTableSeed") ? entry.mData.getInt("LootTableSeed")
                                                                               : FeatureRandom::nextInt(lootRandom);
                level.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(x, y, z)).setLootTable(path, lootSeed);
            }
        }
    }
}

void NetherStructures::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const int32_t x = chunk.getX();
    const int32_t z = chunk.getZ();
    const int64_t seed = generator.getSeed();
    const Box clip = ChunkStructureBounds::chunkBox(x, z, level.getMinY(), level.getMaxY());

    constexpr int32_t REACH = 7;
    for (const StructureChunkPos &start: BASTION.getCandidatesIn(seed, x - REACH, z - REACH, x + REACH, z + REACH)) {
        if (BASTION.isStartChunk(generator, start.x, start.z))
            placeBastion(*cachedBastionPlan(generator, start), level, chunk, clip, StructureFeature::getPostProcessSeed(seed, x, z));
    }

}
