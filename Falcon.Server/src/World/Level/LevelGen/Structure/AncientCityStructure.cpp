#include "World/Level/LevelGen/Structure/AncientCityStructure.h"

#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"
#include "World/Level/LevelGen/Structure/JigsawPlacer.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
    using Box = ChunkStructureBounds::Box;
    using Test = StructureProcessors::Test;
    using Position = std::tuple<int32_t, int32_t, int32_t>;

    const char *const NAME = "minecraft:ancient_city";
    const char *const START = "minecraft:ancient_city_start_degradation";
    const char *const GENERIC = "minecraft:ancient_city_generic_degradation";
    const char *const WALLS = "minecraft:ancient_city_walls_degradation";

    // the start piece reports this height wherever it stands: the city anchor lands one below
    const int32_t START_HEIGHT = -27;
    const int32_t MAX_DEPTH = 7;
    const int32_t MAX_DISTANCE = 128;
    // the pieces stay within the max distance of the start, plus a piece
    const int32_t REACH = 9;

    struct Slot {
        // null: the empty element
        const char *mLocation;
        int32_t mWeight;
        const char *mProcessors;
    };

    struct PoolData {
        const char *mName;
        std::vector<Slot> mSlots;
    };

    // in the game's registration order, every pool falls back to the empty one
    const std::vector<PoolData> POOLS = {
            {"minecraft:ancient_city/city_center", {
                    {"ancient_city/city_center/city_center_1.nbt", 1, START},
                    {"ancient_city/city_center/city_center_2.nbt", 1, START},
                    {"ancient_city/city_center/city_center_3.nbt", 1, START}}},
            {"minecraft:ancient_city/structures", {
                    {nullptr, 7, nullptr},
                    {"ancient_city/structures/barracks.nbt", 4, GENERIC},
                    {"ancient_city/structures/chamber_1.nbt", 4, GENERIC},
                    {"ancient_city/structures/chamber_2.nbt", 4, GENERIC},
                    {"ancient_city/structures/chamber_3.nbt", 4, GENERIC},
                    {"ancient_city/structures/sauna_1.nbt", 4, GENERIC},
                    {"ancient_city/structures/small_statue.nbt", 4, GENERIC},
                    {"ancient_city/structures/large_ruin_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/tall_ruin_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/tall_ruin_2.nbt", 1, GENERIC},
                    {"ancient_city/structures/tall_ruin_3.nbt", 2, GENERIC},
                    {"ancient_city/structures/tall_ruin_4.nbt", 2, GENERIC},
                    {"ancient_city/structures/medium_ruin_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/medium_ruin_2.nbt", 1, GENERIC},
                    {"ancient_city/structures/small_ruin_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/small_ruin_2.nbt", 1, GENERIC},
                    {"ancient_city/structures/large_pillar_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/medium_pillar_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/ice_box_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/camp_1.nbt", 1, GENERIC},
                    {"ancient_city/structures/camp_2.nbt", 1, GENERIC},
                    {"ancient_city/structures/camp_3.nbt", 1, GENERIC}}},
            // the sculk catalyst element: a feature, the empty element after it
            {"minecraft:ancient_city/sculk", {{"", 6, nullptr}, {nullptr, 1, nullptr}}},
            {"minecraft:ancient_city/walls", {
                    {"ancient_city/walls/intact_corner_wall_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_intersection_wall_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_lshape_wall_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_2.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_2.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_3.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_4.nbt", 4, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_passage_1.nbt", 3, WALLS},
                    {"ancient_city/walls/ruined_corner_wall_1.nbt", 1, WALLS},
                    {"ancient_city/walls/ruined_corner_wall_2.nbt", 1, WALLS},
                    {"ancient_city/walls/ruined_horizontal_wall_stairs_1.nbt", 2, WALLS},
                    {"ancient_city/walls/ruined_horizontal_wall_stairs_2.nbt", 2, WALLS},
                    {"ancient_city/walls/ruined_horizontal_wall_stairs_3.nbt", 3, WALLS},
                    {"ancient_city/walls/ruined_horizontal_wall_stairs_4.nbt", 3, WALLS}}},
            {"minecraft:ancient_city/walls/no_corners", {
                    {"ancient_city/walls/intact_horizontal_wall_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_2.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_1.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_2.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_3.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_stairs_4.nbt", 1, WALLS},
                    {"ancient_city/walls/intact_horizontal_wall_bridge.nbt", 1, WALLS}}},
            {"minecraft:ancient_city/city_center/walls", {
                    {"ancient_city/city_center/walls/bottom_1.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/bottom_2.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/bottom_left_corner.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/bottom_right_corner_1.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/bottom_right_corner_2.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/left.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/right.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/top.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/top_right_corner.nbt", 1, GENERIC},
                    {"ancient_city/city_center/walls/top_left_corner.nbt", 1, GENERIC}}},
            {"minecraft:ancient_city/city/entrance", {
                    {"ancient_city/city/entrance/entrance_connector.nbt", 1, GENERIC},
                    {"ancient_city/city/entrance/entrance_path_1.nbt", 1, GENERIC},
                    {"ancient_city/city/entrance/entrance_path_2.nbt", 1, GENERIC},
                    {"ancient_city/city/entrance/entrance_path_3.nbt", 1, GENERIC},
                    {"ancient_city/city/entrance/entrance_path_4.nbt", 1, GENERIC},
                    {"ancient_city/city/entrance/entrance_path_5.nbt", 1, GENERIC}}},
    };

    const char *const SCULK_FEATURE = "minecraft:sculk_patch_feature_ancient_city";

    // the world blocks no degradation rule replaces
    const std::vector<std::string> PROTECTED = {
            "minecraft:mob_spawner",        "minecraft:bedrock",           "minecraft:sculk_sensor",
            "minecraft:redstone_lamp",      "minecraft:lit_redstone_lamp", "minecraft:reinforced_deepslate",
            "minecraft:redstone_wire",      "minecraft:powered_repeater",  "minecraft:unpowered_repeater",
            "minecraft:powered_comparator", "minecraft:unpowered_comparator"};

    // the blocks a twentieth of which crumble to air; one rule per block draws like the game's one rule on the set
    const char *const CRUMBLING[] = {
            "minecraft:deepslate",          "minecraft:deepslate_bricks",       "minecraft:deepslate_tiles",
            "minecraft:cobbled_deepslate",  "minecraft:cracked_deepslate_bricks", "minecraft:cracked_deepslate_tiles",
            "minecraft:deepslate_tile_slab", "minecraft:deepslate_brick_slab",   "minecraft:deepslate_brick_stairs",
            "minecraft:gray_wool"};

    StructureProcessors::Rule rule(const char *input, float chance, const char *output) {
        StructureProcessors::Rule made{Test::RandomBlockMatch, input, chance, nullptr, output, nullptr, 0};
        made.mLocationExcluded = &PROTECTED;
        return made;
    }

    std::vector<StructureProcessors::Rule> processors(const std::string &name) {
        std::vector<StructureProcessors::Rule> rules = {
                rule("minecraft:deepslate_bricks", 0.3f, "minecraft:cracked_deepslate_bricks"),
                rule("minecraft:deepslate_tiles", 0.3f, "minecraft:cracked_deepslate_tiles")};
        if (name == WALLS)
            rules.push_back(rule("minecraft:deepslate_tile_slab", 0.3f, BlockNames::AIR));
        rules.push_back(rule("minecraft:soul_lantern", 0.05f, BlockNames::AIR));
        if (name != START) {
            for (const char *block: CRUMBLING)
                rules.push_back(rule(block, 0.05f, BlockNames::AIR));
        }
        return rules;
    }

    struct CityPools {
        std::vector<std::unique_ptr<JigsawPlacer::Element>> mElements;
        std::map<std::string, JigsawPlacer::Pool> mPools;
        std::map<std::string, std::vector<StructureProcessors::Rule>> mProcessors;

        CityPools() {
            mPools.try_emplace("minecraft:empty");
            for (const char *name: {START, GENERIC, WALLS})
                mProcessors[name] = processors(name);

            for (const PoolData &data: POOLS) {
                JigsawPlacer::Pool &pool = mPools[data.mName];
                pool.mFallback = "minecraft:empty";
                for (const Slot &slot: data.mSlots) {
                    const JigsawPlacer::Element *kept = nullptr;
                    if (slot.mLocation != nullptr) {
                        auto element = std::make_unique<JigsawPlacer::Element>();
                        element->mLocation = slot.mLocation;
                        if (slot.mLocation[0] == '\0')
                            element->mFeature = SCULK_FEATURE;
                        element->mProcessors = slot.mProcessors != nullptr ? slot.mProcessors : "";
                        kept = element.get();
                        mElements.push_back(std::move(element));
                    }
                    pool.mSlots.insert(pool.mSlots.end(), (size_t) slot.mWeight, kept);
                }
            }
        }
    };

    const CityPools &cityPools() {
        static const CityPools pools;
        return pools;
    }

    std::shared_ptr<const JigsawPlacer::Template> templateOf(const JigsawPlacer::Element &element) {
        if (!element.mFeature.empty())
            return JigsawPlacer::featureTemplate();

        static std::mutex mutex;
        static std::map<std::string, std::shared_ptr<const JigsawPlacer::Template>> cache;
        const std::lock_guard lock(mutex);
        const auto found = cache.find(element.mLocation);
        if (found != cache.end())
            return found->second;

        const auto source = StructureTemplate::load(element.mLocation);
        const auto shape = source == nullptr ? nullptr : JigsawPlacer::templateOf(*source);
        cache.emplace(element.mLocation, shape);
        return shape;
    }

    const StructureFeature &feature() {
        static const StructureFeature *city = StructureFeature::get("ancient_city");
        return *city;
    }

    std::vector<JigsawPlacer::Piece> plan(const ChunkGenerator &generator, const StructureChunkPos &start) {
        const CityPools &pools = cityPools();
        std::mt19937 random = feature().createStartRandom(generator.getSeed(), start.x, start.z);
        JigsawPlacer placer(
                [&](int32_t bound) { return FeatureRandom::nextInt(random, bound); },
                [&pools](const std::string &name) -> const JigsawPlacer::Pool * {
                    const auto found = pools.mPools.find(name);
                    return found == pools.mPools.end() ? nullptr : &found->second;
                },
                &templateOf, [](int32_t, int32_t, JigsawPlacer::Projection) { return 0; });

        JigsawPlacer::Settings settings;
        settings.mStartPool = "minecraft:ancient_city/city_center";
        settings.mStartJigsaw = "minecraft:city_anchor";
        settings.mStartX = start.x * 16;
        settings.mStartZ = start.z * 16;
        settings.mStartY = START_HEIGHT;
        // the start is laid at y 0 and the bounds centred there, before it is moved to its height
        settings.mAssemblyOriginY = 0;
        settings.mForwardShuffle = true;
        settings.mMaxDepth = MAX_DEPTH;
        settings.mMaxDistanceHorizontal = MAX_DISTANCE;
        settings.mMaxDistanceVertical = MAX_DISTANCE;
        settings.mDimensionMinY = Dimension::getMinY(DimensionType::Overworld);
        settings.mDimensionHeight = Dimension::getMaxY(DimensionType::Overworld) + 1
                                    - Dimension::getMinY(DimensionType::Overworld);
        JigsawPlacer::Result result;
        if (!placer.place(settings, result))
            return {};
        return std::move(result.mPieces);
    }

    std::shared_ptr<const std::vector<JigsawPlacer::Piece>> cachedPlan(const ChunkGenerator &generator,
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

        const auto made = std::make_shared<const std::vector<JigsawPlacer::Piece>>(plan(generator, start));
        const std::lock_guard lock(mutex);
        if (plans.size() >= 64)
            plans.clear();
        return plans.try_emplace(key, made).first->second;
    }

    std::vector<std::shared_ptr<const std::vector<JigsawPlacer::Piece>>> plansNear(const ChunkGenerator &generator,
                                                                                   int32_t chunkX, int32_t chunkZ) {
        std::vector<std::shared_ptr<const std::vector<JigsawPlacer::Piece>>> plans;
        if (generator.getDimensionType() != DimensionType::Overworld)
            return plans;

        for (const StructureChunkPos &start: feature().getCandidatesIn(generator.getSeed(), chunkX - REACH,
                                                                       chunkZ - REACH, chunkX + REACH,
                                                                       chunkZ + REACH)) {
            if (feature().isStartChunk(generator, start.x, start.z))
                plans.push_back(cachedPlan(generator, start));
        }
        return plans;
    }

    Box boxOf(const JigsawPlacer::Box &box) {
        return {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ};
    }

    void placePiece(const CityPools &pools, const JigsawPlacer::Piece &piece, Level &level, LevelChunk &chunk,
                    const Box &clip, uint32_t seed, std::mt19937 &lootRandom, std::map<Position, BlockState> &placed) {
        const auto shape = StructureTemplate::load(piece.mElement->mLocation);
        if (shape == nullptr)
            return;

        std::vector<const StructureProcessors::Rule *> rules;
        const auto found = pools.mProcessors.find(piece.mElement->mProcessors);
        if (found != pools.mProcessors.end()) {
            for (const StructureProcessors::Rule &rule: found->second)
                rules.push_back(&rule);
        }

        std::mt19937 random(seed);
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
            placed[{x, y, z}] = block;
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

void AncientCityStructure::addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ,
                                    TerrainBeard &beard) {
    for (const auto &planned: plansNear(generator, chunkX, chunkZ)) {
        for (const JigsawPlacer::Piece &piece: *planned) {
            if (!piece.mElement->mFeature.empty())
                continue;
            const JigsawPlacer::Box &box = piece.mBox;
            beard.addPiece(chunkX, chunkZ,
                           {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ, piece.mGroundLevelDelta,
                            TerrainBeard::Adaptation::BeardAndShaver});
        }
    }
}

void AncientCityStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const auto planned = plansNear(generator, chunk.getX(), chunk.getZ());
    if (planned.empty())
        return;

    const CityPools &pools = cityPools();
    const Box clip = ChunkStructureBounds::chunkBox(chunk.getX(), chunk.getZ(), level.getMinY(), level.getMaxY());
    const uint32_t seed = StructureFeature::getPostProcessSeed(generator.getSeed(), chunk.getX(), chunk.getZ());
    StructureTerrain terrain(generator, level.getTerrainCache());
    for (const auto &pieces: planned) {
        if (pieces->empty())
            continue;

        std::mt19937 lootRandom(seed);
        std::map<Position, BlockState> placed;
        std::vector<Box> boxes;
        Box whole = boxOf(pieces->front().mBox);
        for (const JigsawPlacer::Piece &piece: *pieces) {
            if (!piece.mElement->mFeature.empty())
                continue;
            const Box box = boxOf(piece.mBox);
            boxes.push_back(box);
            whole = {std::min(whole.minX, box.minX), std::min(whole.minY, box.minY), std::min(whole.minZ, box.minZ),
                     std::max(whole.maxX, box.maxX), std::max(whole.maxY, box.maxY), std::max(whole.maxZ, box.maxZ)};
            if (box.intersects(clip))
                placePiece(pools, piece, level, chunk, clip, seed, lootRandom, placed);
        }

        // walls meet what stands around them once the pieces are down
        const auto neighbour = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
            const auto found = placed.find({x, y, z});
            return found != placed.end() ? found->second : terrain.getBlock(x, y, z);
        };
        for (const auto &[position, block]: placed) {
            const auto [x, y, z] = position;
            const BlockState shaped = StructureConnections::shaped(block, x, y, z, neighbour);
            if (!(shaped == block))
                chunk.setBlock(x - clip.minX, y, z - clip.minZ, shaped);
        }
        if (whole.intersects(clip))
            chunk.getStructureBounds().addStart(NAME, 0, whole, boxes, clip);
    }
}
