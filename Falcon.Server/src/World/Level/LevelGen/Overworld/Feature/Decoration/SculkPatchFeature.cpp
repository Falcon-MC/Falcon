#include "World/Level/LevelGen/Overworld/Feature/Decoration/SculkPatchFeature.h"

#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/Sculk/SculkSpreader.h"
#include "World/Level/Sculk/SculkWorld.h"

namespace {
    // the deep dark's features/sculk_patch_feature.json; extra_growth_chance is absent and reads 0 to 0
    const int32_t CURSOR_COUNT = 10;
    const int32_t CHARGE_AMOUNT = 32;
    const int32_t SPREAD_ATTEMPTS = 64;
    const int32_t GROWTH_ROUNDS = 0;
    const int32_t SPREAD_ROUNDS = 1;
    const float CENTRAL_BLOCK_CHANCE = 0.5f;

    // the world generation spreader of the game
    const SculkSpreader::Settings SETTINGS = {true, 50, 1, 5, 10};

    const BlockState &airState() {
        static const BlockState air(BlockNames::AIR);
        return air;
    }

    bool isWater(const BlockState &state) {
        return state.mName == BlockNames::WATER || state.mName == BlockNames::FLOWING_WATER;
    }

    // a decoration's blocks: a vein keeps the water it grows in as its second block
    class DecorationSculkWorld : public SculkWorld {
    public:
        explicit DecorationSculkWorld(BlockManager &blocks) : mBlocks(blocks) {}

        const BlockState &getBlock(const Vector3i &position) override {
            return mBlocks.getBlockAt(position.x, position.y, position.z);
        }

        // as in a level: air taking a waterlogged block's place lets its water back to the first layer
        void setBlock(const Vector3i &position, const BlockState &state) override {
            const BlockState &replaced = mBlocks.getBlockAt(position.x, position.y, position.z);
            const BlockState &extra = mBlocks.getExtraBlockAt(position.x, position.y, position.z);
            const BlockState water = isWater(replaced) ? replaced : extra;
            if (!isWater(water)) {
                mBlocks.setBlockStateAt(position.x, position.y, position.z, state);
                return;
            }

            const bool holdsWater = !isWater(state) && LiquidPhysicsSystem::getWaterloggingLevel(state) > 0;
            mBlocks.setBlockStateAt(position.x, position.y, position.z,
                                    DecorationSupport::isAir(state) && isWater(extra) ? extra : state);
            if (holdsWater)
                mBlocks.setExtraBlockAt(position.x, position.y, position.z, water);
            else if (isWater(extra))
                mBlocks.setExtraBlockAt(position.x, position.y, position.z, airState());
        }

        bool isWaterSource(const Vector3i &position) override {
            const BlockState &state = mBlocks.getBlockAt(position.x, position.y, position.z);
            const BlockState &extra = mBlocks.getExtraBlockAt(position.x, position.y, position.z);
            return (isWater(state) && WaterBlock::isSource(state)) || (isWater(extra) && WaterBlock::isSource(extra));
        }

    private:
        BlockManager &mBlocks;
    };

    // air or a water source, against a solid block on one side; or sculk and veins already
    bool canSpreadFrom(DecorationSculkWorld &world, const Vector3i &origin) {
        const BlockState state = world.getBlock(origin);
        if (state.mName == BlockNames::SCULK || state.mName == BlockNames::SCULK_VEIN)
            return true;
        if (!DecorationSupport::isAir(state) && !(isWater(state) && WaterBlock::isSource(state)))
            return false;

        static const Vector3i SIDES[6] = {Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 0, -1),
                                          Vector3i(0, 0, 1), Vector3i(0, -1, 0), Vector3i(0, 1, 0)};
        for (const Vector3i &side: SIDES) {
            const Vector3i next(origin.x + side.x, origin.y + side.y, origin.z + side.z);
            if (BlockSupport::isSolid(world.getBlock(next)))
                return true;
        }
        return false;
    }
}

bool SculkPatchFeature::place(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    DecorationSculkWorld world(blocks);
    const Vector3i origin(x, y, z);
    if (!canSpreadFrom(world, origin))
        return false;

    SculkSpreader spreader(SETTINGS);
    for (int32_t round = 0; round < GROWTH_ROUNDS + SPREAD_ROUNDS; round++) {
        for (int32_t cursor = 0; cursor < CURSOR_COUNT; cursor++)
            spreader.addCursors(origin, CHARGE_AMOUNT);
        for (int32_t attempt = 0; attempt < SPREAD_ATTEMPTS; attempt++)
            spreader.updateCursors(world, origin, random, round < SPREAD_ROUNDS);
        spreader.clearCursors();
    }

    const Vector3i below(x, y - 1, z);
    if (CENTRAL_BLOCK_CHANCE >= GameRandom::nextFloat(random) && BlockSupport::isSolid(world.getBlock(below)))
        world.setBlock(origin, VanillaBlocks::SCULK_CATALYST().toBlockState());

    // the extra shriekers: 0 to 0 still draws its count
    GameRandom::nextInt(random, 1);
    return true;
}
