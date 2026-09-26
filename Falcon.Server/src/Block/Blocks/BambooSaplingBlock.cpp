#include "Block/Blocks/BambooSaplingBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/BambooBlock.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(BambooSaplingBlock, 313);

using namespace PlantGrowthHelpers;

bool BambooSaplingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bamboo_sapling";
}

void BambooSaplingBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) owner;

    const Vector3i top = above(position);
    if (state.mStates.getBool(AGE_BIT, false) || !isInRange(level, top) || !isAirAt(level, top))
        return;

    if (RandomTickSystem::getFullLight(level, top) < BAMBOO_MIN_LIGHT || RandomTickSystem::nextInt(3) != 0)
        return;

    BlockChangeSystem::change(level, top, DecorationSupport::withState(VanillaBlocks::BAMBOO().toBlockState(),
                                                                       LEAF_SIZE, "small_leaves"),
                              BlockChangeCause::Grow, true);
}

void BambooSaplingBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                            const BlockState &state) const {
    PlantBlock::onNeighbourChanged(owner, level, position, state);

    const BlockState top = stateAt(level, above(position));
    if (!BambooBlock::matches(top.mName) || !matches(stateAt(level, position).mName))
        return;

    const std::string thickness = top.mStates.getString(STALK_THICKNESS, "thin");
    level.setBlock(position, DecorationSupport::withState(VanillaBlocks::BAMBOO().toBlockState(), STALK_THICKNESS,
                                                          thickness.c_str()), true);
}
