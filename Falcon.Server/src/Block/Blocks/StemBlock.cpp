#include "Block/Blocks/StemBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(StemBlock, 250);

#include "Block/Blocks/GrowthHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Feature/IFeature.h"
#include "Level/Level.h"

using namespace GrowthHelpers;

namespace {
    const char *STEM_FACING = "facing_direction";
    const int32_t STEM_FACING_DOWN = 0;
    const int32_t STEM_FACING_NORTH = 2;
    const int32_t STEM_FACING_EAST = 5;
    const int STEM_SIDES[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
}

bool StemBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:pumpkin_stem" || identifier == "minecraft:melon_stem";
}

BlockState StemBlock::getFruitState() const {
    return getIdentifier() == "minecraft:melon_stem" ? VanillaBlocks::MELON_BLOCK().toBlockState()
                                                     : VanillaBlocks::PUMPKIN().toBlockState();
}

void StemBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
    CropBlock::onNeighbourChanged(owner, level, position, state);

    const BlockState current = level.getBlockState(position.x, position.y, position.z);
    if (current.mName != state.mName)
        return;

    const int32_t facing = current.mStates.getInt(STEM_FACING, STEM_FACING_DOWN);
    if (facing < STEM_FACING_NORTH || facing > STEM_FACING_EAST)
        return;

    const int32_t side = facing - STEM_FACING_NORTH;
    const BlockState fruit = level.getBlockState(position.x + STEM_SIDES[side][0], position.y,
                                                 position.z + STEM_SIDES[side][1]);
    if (fruit.mName != getFruitState().mName)
        level.setBlock(position, withState(current, STEM_FACING, STEM_FACING_DOWN), false);
}

void StemBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    if (RandomTickSystem::nextInt(2) != 0)
        return;

    if (RandomTickSystem::getFullLight(level, position) < MINIMUM_LIGHT_LEVEL)
        return;

    const int32_t growth = state.mStates.getInt("growth");
    if (growth < 7) {
        BlockChangeSystem::change(level, position, withState(state, "growth", growth + 1), BlockChangeCause::Grow,
                                  false);
        return;
    }

    const BlockState fruit = getFruitState();

    for (const auto &side: STEM_SIDES) {
        const BlockState neighbour = level.getBlockState(position.x + side[0], position.y, position.z + side[1]);
        if (neighbour.mName == fruit.mName)
            return;
    }

    const int chosen = RandomTickSystem::nextInt(4);
    const Vector3i target(position.x + STEM_SIDES[chosen][0], position.y, position.z + STEM_SIDES[chosen][1]);

    if (level.getBlockState(target.x, target.y, target.z).mName != "minecraft:air")
        return;

    const BlockState below = level.getBlockState(target.x, target.y - 1, target.z);
    if (!IFeature::isSupportDirt(below) && below.mName != "minecraft:farmland")
        return;

    if (!BlockChangeSystem::change(level, target, fruit, BlockChangeCause::Grow, true))
        return;
    level.setBlock(position, withState(state, STEM_FACING, STEM_FACING_NORTH + chosen), false);
}
