#include "World/Level/LevelGen/Structure/RuinedPortalStructure.h"

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace {
    const int32_t SEA_LEVEL = 63;
    // the spread netherrack and the box reach two chunks from the start at most
    const int32_t REACH = 2;
    const size_t MAX_CACHED_PLANS = 64;

    const float GIANT_CHANCE = 0.05f;
    const float MAGMA_CHANCE = 0.05f;
    // the netherrack spread around the middle, by distance
    const std::array<float, 14> SPREAD = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                          0.9f, 0.9f, 0.8f, 0.7f, 0.6f, 0.4f, 0.2f};

    const std::array<const char *, 10> PORTALS = {
            "ruined_portal/portal_1", "ruined_portal/portal_2", "ruined_portal/portal_3", "ruined_portal/portal_4",
            "ruined_portal/portal_5", "ruined_portal/portal_6", "ruined_portal/portal_7", "ruined_portal/portal_8",
            "ruined_portal/portal_9", "ruined_portal/portal_10"};
    const std::array<const char *, 3> GIANT_PORTALS = {"ruined_portal/giant_portal_1", "ruined_portal/giant_portal_2",
                                                       "ruined_portal/giant_portal_3"};

    // the game's block lists, then the blocks compared one by one
    const std::array<const char *, 14> FULL_BLOCKS = {"minecraft:stone_bricks",
                                                      "minecraft:mossy_stone_bricks",
                                                      "minecraft:cracked_stone_bricks",
                                                      "minecraft:chiseled_stone_bricks",
                                                      "minecraft:smooth_stone_double_slab",
                                                      "minecraft:sandstone_double_slab",
                                                      "minecraft:petrified_oak_double_slab",
                                                      "minecraft:cobblestone_double_slab",
                                                      "minecraft:brick_double_slab",
                                                      "minecraft:stone_brick_double_slab",
                                                      "minecraft:quartz_double_slab",
                                                      "minecraft:nether_brick_double_slab",
                                                      "minecraft:oak_double_slab",
                                                      "minecraft:stone"};
    // the game's list then oak_slab
    const std::array<const char *, 9> SLABS = {
            "minecraft:smooth_stone_slab", "minecraft:sandstone_slab",    "minecraft:petrified_oak_slab",
            "minecraft:cobblestone_slab",  "minecraft:brick_slab",        "minecraft:stone_brick_slab",
            "minecraft:quartz_slab",       "minecraft:nether_brick_slab", "minecraft:oak_slab"};
    const std::array<const char *, 14> WALLS = {
            "minecraft:cobblestone_wall",   "minecraft:mossy_cobblestone_wall", "minecraft:granite_wall",
            "minecraft:diorite_wall",       "minecraft:andesite_wall",          "minecraft:sandstone_wall",
            "minecraft:brick_wall",         "minecraft:stone_brick_wall",       "minecraft:mossy_stone_brick_wall",
            "minecraft:nether_brick_wall",  "minecraft:end_stone_brick_wall",   "minecraft:prismarine_wall",
            "minecraft:red_sandstone_wall", "minecraft:red_nether_brick_wall"};

    using Position = std::tuple<int32_t, int32_t, int32_t>;
    using PlanKey = std::tuple<int64_t, int32_t, int32_t>;

    std::mutex plansMutex;
    std::map<PlanKey, std::shared_ptr<const RuinedPortalStructure::Plan>> plans;

    template <size_t N>
    bool isOneOf(const std::array<const char *, N> &names, const std::string &name) {
        return std::find_if(names.begin(), names.end(), [&name](const char *entry) { return name == entry; })
               != names.end();
    }

    BlockState defaultState(const char *name) {
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        return defaults != nullptr ? BlockState(name, *defaults) : BlockState(name);
    }

    BlockState waterState() {
        Tag states = Tag::ofCompound();
        states.putInt("liquid_depth", 0);
        return BlockState(BlockNames::WATER, states);
    }

    BlockState stairs(const char *name, int32_t direction, bool upsideDown) {
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        Tag states = defaults != nullptr ? *defaults : Tag::ofCompound();
        states.putInt("weirdo_direction", direction);
        states.putByte("upside_down_bit", upsideDown ? 1 : 0);
        return BlockState(name, states);
    }

    // any side, either half
    BlockState randomStairs(std::mt19937 &random, const char *name) {
        const int32_t direction = FeatureRandom::nextInt(random, 4);
        const bool upsideDown = FeatureRandom::nextFloat(random) < 0.5f;
        return stairs(name, direction, upsideDown);
    }

    std::string lootTablePath(const std::string &name) {
        const size_t colon = name.find(':');
        return "loot_tables/" + (colon == std::string::npos ? name : name.substr(colon + 1)) + ".json";
    }

    bool isLava(const BlockState &block) {
        return block.mName == BlockNames::LAVA || block.mName == BlockNames::FLOWING_LAVA;
    }

    int32_t middle(int32_t min, int32_t max) {
        return min + (max - min + 1) / 2;
    }

    // the portal's own view of the world: its blocks over the bare terrain
    class World {
    public:
        explicit World(StructureTerrain &terrain) : mTerrain(terrain) {}

        const BlockState &get(int32_t x, int32_t y, int32_t z) {
            static const BlockState air(BlockNames::AIR);
            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                return air;
            const auto placed = mBlocks.find({x, y, z});
            return placed != mBlocks.end() ? placed->second : mTerrain.getBlock(x, y, z);
        }

        void set(int32_t x, int32_t y, int32_t z, const BlockState &block) {
            if (y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y)
                mBlocks[{x, y, z}] = block;
        }

        void setWater(int32_t x, int32_t y, int32_t z) {
            if (y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y)
                mWater.insert(Position(x, y, z));
        }

        // above the top solid block, without leaves; water stops it only when asked
        int32_t aboveTopSolid(int32_t x, int32_t z, bool waterStops) {
            for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; --y) {
                if (_stops(get(x, y, z), waterStops))
                    return y + 1;
            }
            return LevelChunk::MIN_Y;
        }

        // a corner holding the portal up: the same rule as the height above
        bool isSolid(int32_t x, int32_t y, int32_t z, bool waterSolid) {
            return _stops(get(x, y, z), waterSolid);
        }

        const std::map<Position, BlockState> &getBlocks() const {
            return mBlocks;
        }

        std::vector<Position> getWater() const {
            return std::vector<Position>(mWater.begin(), mWater.end());
        }

    private:
        static bool _stops(const BlockState &block, bool waterStops) {
            BlockMaterialType type = BlockMaterialType::Air;
            if (!BlockMaterial::find(block, type) || type == BlockMaterialType::Leaves)
                return false;
            if (type == BlockMaterialType::Water && waterStops)
                return true;
            return (BlockMaterial::flagsOf(type) & BlockMaterial::FLAG_SOLID) != 0;
        }

        StructureTerrain &mTerrain;
        std::map<Position, BlockState> mBlocks;
        std::set<Position> mWater;
    };

    // the block and up to eight more under it, each one a coin flip
    void dripColumn(World &world, std::mt19937 &random, int32_t x, int32_t y, int32_t z) {
        static const BlockState netherrack = defaultState(BlockNames::NETHERRACK);
        static const BlockState magma = defaultState(BlockNames::MAGMA);
        world.set(x, y, z, FeatureRandom::nextFloat(random) < MAGMA_CHANCE ? magma : netherrack);
        for (int32_t below = 1; below <= 8; ++below) {
            if (FeatureRandom::nextFloat(random) >= 0.5f)
                return;
            world.set(x, y - below, z, FeatureRandom::nextFloat(random) < MAGMA_CHANCE ? magma : netherrack);
        }
    }

    // a gold block by lava turns into lava, else into air
    void ageGold(World &world, std::mt19937 &random, int32_t x, int32_t y, int32_t z) {
        if ((double) FeatureRandom::nextFloat(random) >= 0.3)
            return;
        const bool byLava = isLava(world.get(x - 1, y, z)) || isLava(world.get(x + 1, y, z))
                            || isLava(world.get(x, y, z - 1)) || isLava(world.get(x, y, z + 1))
                            || isLava(world.get(x, y + 1, z));
        world.set(x, y, z, byLava ? defaultState(BlockNames::LAVA) : BlockState(BlockNames::AIR));
    }

    class PortalBuilder {
    public:
        PortalBuilder(const ChunkGenerator &generator, int32_t startX, int32_t startZ, TerrainCache *cache)
                : mGenerator(generator), mTerrain(generator, cache), mWorld(mTerrain),
                  mRandom(StructureFeature::createPostProcessRandom(generator.getSeed(), startX, startZ)),
                  mOriginX(startX * 16), mOriginZ(startZ * 16) {
            // createStart keeps the biome over the preliminary surface
            const int32_t height = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
            mBiome = generator.pickBiome(mOriginX + 8, height, mOriginZ + 8);
        }

        std::shared_ptr<RuinedPortalStructure::Plan> build() {
            auto plan = std::make_shared<RuinedPortalStructure::Plan>();
            const bool ice = BiomeChunkGenDataRegistry::hasTag(mBiome, "ice");
            const float placement = FeatureRandom::nextFloat(mRandom);
            const bool desert = BiomeChunkGenDataRegistry::hasTag(mBiome, "desert");
            mOceanOrSwamp = BiomeChunkGenDataRegistry::hasTag(mBiome, "ocean")
                            || BiomeChunkGenDataRegistry::hasTag(mBiome, "swamp");
            const bool landStopsOnWater = !mOceanOrSwamp;
            const bool lowPlacement = placement < 0.5f;

            plan->mQuarterTurns = FeatureRandom::nextInt(mRandom, 4);
            plan->mMirror = FeatureRandom::nextFloat(mRandom) >= 0.5f ? 1 : 0;
            const int32_t originHeight = mWorld.aboveTopSolid(mOriginX, mOriginZ, landStopsOnWater);
            if (FeatureRandom::nextFloat(mRandom) < GIANT_CHANCE)
                plan->mTemplate =
                        GIANT_PORTALS[(size_t) FeatureRandom::nextInt(mRandom, (int32_t) GIANT_PORTALS.size())];
            else
                plan->mTemplate = PORTALS[(size_t) FeatureRandom::nextInt(mRandom, (int32_t) PORTALS.size())];

            const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(plan->mTemplate + ".nbt");
            if (shape == nullptr)
                return nullptr;

            _box(*plan, *shape, originHeight);
            const int32_t middleHeight = mWorld.aboveTopSolid(middle(plan->mMinX, plan->mMaxX),
                                                              middle(plan->mMinZ, plan->mMaxZ), landStopsOnWater);
            int32_t y = middleHeight - 1;
            if (lowPlacement) {
                const int32_t biome = mGenerator.pickBiome(middle(plan->mMinX, plan->mMaxX),
                                                           middle(plan->mMinY, plan->mMaxY),
                                                           middle(plan->mMinZ, plan->mMaxZ));
                // the dimension's sea level (+204) or its floor (+58)
                const int32_t lowest = BiomeChunkGenDataRegistry::hasTag(biome, "mountain")
                                                       || BiomeChunkGenDataRegistry::hasTag(biome, "extreme_hills")
                                               ? SEA_LEVEL + 7
                                               : LevelChunk::MIN_Y + 15;
                y = middleHeight - (plan->mMaxY - plan->mMinY) - 2;
                if (y > lowest)
                    y = lowest + FeatureRandom::nextInt(mRandom, y - lowest + 1);
            } else if (desert) {
                y = middleHeight - (plan->mMaxY - plan->mMinY) + FeatureRandom::nextInt(mRandom, 6);
            }
            _moveTo(*plan, y);
            _settle(*plan);

            _spreadNetherrack(*plan, lowPlacement || desert);

            // the template air digs a pocket unless it is ignored
            const bool keepAir = lowPlacement || (!desert && FeatureRandom::nextFloat(mRandom) < 0.5f);

            const bool waterlogged = _isUnderWater(*plan);
            _placeTemplate(*plan, *shape, keepAir, waterlogged);

            for (int32_t x = plan->mMinX + 1; x < plan->mMaxX; ++x) {
                for (int32_t z = plan->mMinZ + 1; z < plan->mMaxZ; ++z) {
                    if (mWorld.get(x, plan->mMinY, z).mName == BlockNames::NETHERRACK)
                        dripColumn(mWorld, mRandom, x, plan->mMinY - 1, z);
                }
            }

            float mossiness = 0.2f;
            if (waterlogged)
                mossiness = 0.8f;
            else if (desert)
                mossiness = 0.0f;
            else if (BiomeChunkGenDataRegistry::hasTag(mBiome, "jungle"))
                mossiness = 0.8f;
            else if (BiomeChunkGenDataRegistry::hasTag(mBiome, "swamp"))
                mossiness = 0.5f;
            _age(*plan, mossiness, ice);
            _shapeStairs(*plan);

            for (const auto &entry: mWorld.getBlocks())
                plan->mChanges.push_back({std::get<0>(entry.first), std::get<1>(entry.first), std::get<2>(entry.first),
                                          0, entry.second});
            for (const Position &water: mWorld.getWater())
                plan->mChanges.push_back({std::get<0>(water), std::get<1>(water), std::get<2>(water), 1, waterState()});
            return plan;
        }

    private:
        // the box reaches the template size past the origin, one block more than the template itself
        void _box(RuinedPortalStructure::Plan &plan, const StructureTemplate &shape, int32_t originHeight) const {
            const int32_t sizeX = shape.getSizeX();
            const int32_t sizeZ = plan.mMirror != 0 ? -shape.getSizeZ() : shape.getSizeZ();
            int32_t reachX = sizeX;
            int32_t reachZ = sizeZ;
            switch (plan.mQuarterTurns) {
                case 1:
                    reachX = -sizeZ;
                    reachZ = sizeX;
                    break;
                case 2:
                    reachX = -sizeX;
                    reachZ = -sizeZ;
                    break;
                case 3:
                    reachX = sizeZ;
                    reachZ = -sizeX;
                    break;
                default:
                    break;
            }
            plan.mMinX = std::min(mOriginX, mOriginX + reachX);
            plan.mMaxX = std::max(mOriginX, mOriginX + reachX);
            plan.mMinZ = std::min(mOriginZ, mOriginZ + reachZ);
            plan.mMaxZ = std::max(mOriginZ, mOriginZ + reachZ);
            plan.mMinY = originHeight - 1;
            plan.mMaxY = originHeight - 1 + shape.getSizeY();
        }

        static void _moveTo(RuinedPortalStructure::Plan &plan, int32_t y) {
            plan.mMaxY += y - plan.mMinY;
            plan.mMinY = y;
        }

        // down until three of the four corners under the box hold it
        void _settle(RuinedPortalStructure::Plan &plan) {
            const bool waterSolid = !mOceanOrSwamp;
            while (plan.mMinY > LevelChunk::MIN_Y + 15) {
                const int32_t below = plan.mMinY - 1;
                int32_t solid = 0;
                solid += mWorld.isSolid(plan.mMinX, below, plan.mMinZ, waterSolid) ? 1 : 0;
                solid += mWorld.isSolid(plan.mMinX, below, plan.mMaxZ, waterSolid) ? 1 : 0;
                solid += mWorld.isSolid(plan.mMaxX, below, plan.mMinZ, waterSolid) ? 1 : 0;
                solid += mWorld.isSolid(plan.mMaxX, below, plan.mMaxZ, waterSolid) ? 1 : 0;
                if (solid >= 3)
                    return;
                _moveTo(plan, plan.mMinY - 1);
            }
        }

        void _spreadNetherrack(const RuinedPortalStructure::Plan &plan, bool underBox) {
            static const BlockState netherrack = defaultState(BlockNames::NETHERRACK);
            static const BlockState magma = defaultState(BlockNames::MAGMA);
            const int32_t span = (plan.mMaxX - plan.mMinX) + (plan.mMaxZ - plan.mMinZ) + 2;
            const int32_t shift = FeatureRandom::nextInt(mRandom, std::max(8 - span / 4, 1));
            const int32_t centerX = middle(plan.mMinX, plan.mMaxX);
            const int32_t centerZ = middle(plan.mMinZ, plan.mMaxZ);
            const int32_t last = (int32_t) SPREAD.size();
            for (int32_t x = centerX - last; x <= centerX + last; ++x) {
                for (int32_t z = centerZ - last; z <= centerZ + last; ++z) {
                    const int32_t distance = std::abs(x - centerX) + std::abs(z - centerZ) + shift;
                    if (distance >= last)
                        continue;
                    if (!(FeatureRandom::nextDouble(mRandom) < (double) SPREAD[(size_t) std::max(distance, 0)]))
                        continue;

                    const int32_t surface = mWorld.aboveTopSolid(x, z, !mOceanOrSwamp) - 1;
                    const int32_t y = underBox ? std::min(plan.mMinY, surface) : surface;
                    if (std::abs(y - plan.mMinY) > 3)
                        continue;
                    mWorld.set(x, y, z, FeatureRandom::nextFloat(mRandom) < MAGMA_CHANCE ? magma : netherrack);
                    dripColumn(mWorld, mRandom, x, y - 1, z);
                }
            }
        }

        // worlds from 1.18.0 on: any corner of the box in water
        bool _isUnderWater(const RuinedPortalStructure::Plan &plan) {
            for (const int32_t x: {plan.mMinX, plan.mMaxX}) {
                for (const int32_t y: {plan.mMinY, plan.mMaxY}) {
                    for (const int32_t z: {plan.mMinZ, plan.mMaxZ}) {
                        if (mWorld.get(x, y, z).mName == BlockNames::WATER)
                            return true;
                    }
                }
            }
            return false;
        }

        // placed with full integrity,
        // structure and jigsaw blocks left out
        void _placeTemplate(RuinedPortalStructure::Plan &plan, const StructureTemplate &shape, bool keepAir,
                            bool waterlogged) {
            for (const StructureTemplate::BlockEntry &entry: shape.getBlocks()) {
                const StructureTemplate::PaletteEntry &palette = shape.getPalette()[(size_t) entry.mPaletteIndex];
                if (palette.mName == "minecraft:structure_block" || palette.mName == "minecraft:jigsaw")
                    continue;
                if (!keepAir && palette.mName == BlockNames::AIR)
                    continue;

                BlockState block;
                if (!StructureBlockTable::translate(palette, plan.mQuarterTurns, plan.mMirror, block))
                    continue;

                int32_t x = entry.x;
                int32_t z = entry.z;
                StructureTemplate::mirror(plan.mMirror, x, z);
                StructureTemplate::rotate(plan.mQuarterTurns, x, z);
                x += mOriginX;
                z += mOriginZ;
                const int32_t y = plan.mMinY + entry.y;
                if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                    continue;

                mWorld.set(x, y, z, block);
                if (waterlogged && y < SEA_LEVEL && LiquidPhysicsSystem::getWaterloggingLevel(block) > 0)
                    mWorld.setWater(x, y, z);
                if (entry.mData.getString("id", std::string()) == "minecraft:chest"
                    && block.mName == BlockNames::CHEST) {
                    // placing a chest draws its seed
                    const int32_t seed = FeatureRandom::nextInt(mRandom);
                    const std::string table = entry.mData.getString("LootTable", std::string());
                    if (!table.empty())
                        plan.mChests.push_back({x, y, z, lootTablePath(table), seed});
                }
            }
        }

        // the blocks of the box age once the template is down, x then y then z
        void _age(const RuinedPortalStructure::Plan &plan, float mossiness, bool ice) {
            for (int32_t x = plan.mMinX; x <= plan.mMaxX; ++x) {
                for (int32_t y = plan.mMinY; y <= plan.mMaxY; ++y) {
                    for (int32_t z = plan.mMinZ; z <= plan.mMaxZ; ++z)
                        _ageBlock(x, y, z, mossiness, ice);
                }
            }
        }

        void _ageBlock(int32_t x, int32_t y, int32_t z, float mossiness, bool ice) {
            const BlockState block = mWorld.get(x, y, z);
            const std::string &name = block.mName;
            if (isOneOf(FULL_BLOCKS, name)) {
                if (FeatureRandom::nextFloat(mRandom) < 0.5f)
                    return;
                const std::array<BlockState, 2> plain = {defaultState("minecraft:cracked_stone_bricks"),
                                                         randomStairs(mRandom, "minecraft:stone_brick_stairs")};
                const std::array<BlockState, 2> mossy = {defaultState("minecraft:mossy_stone_bricks"),
                                                         randomStairs(mRandom, "minecraft:mossy_stone_brick_stairs")};
                _setEither(x, y, z, mossiness, plain, mossy);
                return;
            }

            if (name == "minecraft:stone_brick_stairs") {
                const int32_t direction = block.mStates.getInt("weirdo_direction", 0);
                const bool upsideDown = block.mStates.getByte("upside_down_bit", 0) != 0;
                if (FeatureRandom::nextFloat(mRandom) < 0.5f)
                    return;
                const std::array<BlockState, 2> plain = {defaultState("minecraft:normal_stone_slab"),
                                                         defaultState("minecraft:stone_brick_slab")};
                const std::array<BlockState, 2> mossy = {
                        stairs("minecraft:mossy_stone_brick_stairs", direction, upsideDown),
                        defaultState("minecraft:mossy_stone_brick_slab")};
                _setEither(x, y, z, mossiness, plain, mossy);
                return;
            }

            if (isOneOf(SLABS, name)) {
                if (mossiness > FeatureRandom::nextFloat(mRandom))
                    mWorld.set(x, y, z, defaultState("minecraft:mossy_stone_brick_slab"));
                return;
            }

            if (isOneOf(WALLS, name)) {
                if (mossiness > FeatureRandom::nextFloat(mRandom))
                    mWorld.set(x, y, z, defaultState("minecraft:mossy_stone_brick_wall"));
                return;
            }

            // iron bars only turn into chains in the nether
            if (name == "minecraft:iron_bars")
                return;

            if (name == BlockNames::NETHERRACK) {
                if (!ice && (double) FeatureRandom::nextFloat(mRandom) < 0.1)
                    mWorld.set(x, y, z, defaultState(BlockNames::MAGMA));
                return;
            }

            if (name == BlockNames::OBSIDIAN) {
                if ((double) FeatureRandom::nextFloat(mRandom) < 0.2)
                    mWorld.set(x, y, z, defaultState(BlockNames::CRYING_OBSIDIAN));
                return;
            }

            if (isLava(block)) {
                if (mOceanOrSwamp)
                    mWorld.set(x, y, z, defaultState(BlockNames::MAGMA));
                else if (ice)
                    mWorld.set(x, y, z, defaultState(BlockNames::NETHERRACK));
                else if ((double) FeatureRandom::nextFloat(mRandom) < 0.2)
                    mWorld.set(x, y, z, defaultState(BlockNames::MAGMA));
                return;
            }

            if (name == BlockNames::GOLD_BLOCK)
                ageGold(mWorld, mRandom, x, y, z);
        }

        // a mossy pick when the draw falls under the mossiness, the index drawn first either way
        void _setEither(int32_t x, int32_t y, int32_t z, float mossiness, const std::array<BlockState, 2> &plain,
                        const std::array<BlockState, 2> &mossy) {
            const float chance = FeatureRandom::nextFloat(mRandom);
            const size_t index = (size_t) FeatureRandom::nextInt(mRandom, 2);
            mWorld.set(x, y, z, mossiness > chance ? mossy[index] : plain[index]);
        }

        // the corner of every stair in the box, from its neighbours
        void _shapeStairs(const RuinedPortalStructure::Plan &plan) {
            const StructureConnections::Lookup at = [this](int32_t x, int32_t y, int32_t z) {
                return mWorld.get(x, y, z);
            };
            for (int32_t x = plan.mMinX; x <= plan.mMaxX; ++x) {
                for (int32_t y = plan.mMinY; y <= plan.mMaxY; ++y) {
                    for (int32_t z = plan.mMinZ; z <= plan.mMaxZ; ++z) {
                        const BlockState block = mWorld.get(x, y, z);
                        if (!block.mStates.contains("minecraft:corner"))
                            continue;
                        const BlockState shaped = StructureConnections::shaped(block, x, y, z, at);
                        if (!(shaped == block))
                            mWorld.set(x, y, z, shaped);
                    }
                }
            }
        }

        const ChunkGenerator &mGenerator;
        StructureTerrain mTerrain;
        World mWorld;
        std::mt19937 mRandom;
        int32_t mOriginX;
        int32_t mOriginZ;
        int32_t mBiome = 0;
        bool mOceanOrSwamp = false;
    };
}

std::shared_ptr<const RuinedPortalStructure::Plan>
RuinedPortalStructure::plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ, TerrainCache *cache) {
    const PlanKey key(generator.getSeed(), startX, startZ);
    {
        const std::lock_guard<std::mutex> lock(plansMutex);
        const auto found = plans.find(key);
        if (found != plans.end())
            return found->second;
    }

    PortalBuilder builder(generator, startX, startZ, cache);
    const std::shared_ptr<const Plan> built = builder.build();
    const std::lock_guard<std::mutex> lock(plansMutex);
    if (plans.size() >= MAX_CACHED_PLANS)
        plans.clear();
    plans.emplace(key, built);
    return built;
}

void RuinedPortalStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *portals = StructureFeature::get("ruined_portal");
    for (int32_t startX = chunk.getX() - REACH; startX <= chunk.getX() + REACH; ++startX) {
        for (int32_t startZ = chunk.getZ() - REACH; startZ <= chunk.getZ() + REACH; ++startZ) {
            if (!portals->isStartChunk(generator, startX, startZ))
                continue;
            const std::shared_ptr<const Plan> portal = plan(generator, startX, startZ, level.getTerrainCache());
            if (portal == nullptr)
                continue;

            for (const Change &change: portal->mChanges) {
                if ((change.x >> 4) == chunk.getX() && (change.z >> 4) == chunk.getZ())
                    chunk.setBlock(change.x & 15, change.y, change.z & 15, change.mLayer, change.mBlock);
            }
            for (const Chest &chest: portal->mChests) {
                if ((chest.x >> 4) != chunk.getX() || (chest.z >> 4) != chunk.getZ())
                    continue;
                level.getBlockActors()
                        .getOrCreate<ChestBlockActor>(Vector3i(chest.x, chest.y, chest.z))
                        .setLootTable(chest.mLootTable, chest.mSeed);
            }
        }
    }
}
