#include "Block/Blocks/BambooBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(BambooBlock, 314);

using namespace PlantGrowthHelpers;

namespace {
    const int32_t MAX_BAMBOO_HEIGHT = 16;
    const int32_t BAMBOO_SLOWDOWN_HEIGHT = 11;
}

bool BambooBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bamboo";
}

void BambooBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) owner;

    const Vector3i top = above(position);
    if (state.mStates.getBool(AGE_BIT, false) || !isInRange(level, top) || !isAirAt(level, top))
        return;

    if (RandomTickSystem::getFullLight(level, top) < BAMBOO_MIN_LIGHT)
        return;

    const int32_t height = countStalkBelow(level, position) + 1;
    if (height < MAX_BAMBOO_HEIGHT && RandomTickSystem::nextInt(3) == 0)
        grow(level, position, state, height);
}

int32_t BambooBlock::countStalkBelow(Level &level, const Vector3i &position) {
    int32_t count = 0;
    Vector3i current = below(position);
    while (count < MAX_BAMBOO_HEIGHT && matches(stateAt(level, current).mName)) {
        ++count;
        current = below(current);
    }
    return count;
}

void BambooBlock::grow(Level &level, const Vector3i &position, const BlockState &state, int32_t height) {
    const Vector3i underPosition = below(position);
    const Vector3i underUnderPosition = below(underPosition);
    const BlockState under = stateAt(level, underPosition);
    const BlockState underUnder = stateAt(level, underUnderPosition);

    const char *leaves = "no_leaves";
    if (height >= 1) {
        if (!matches(under.mName) || under.mStates.getString(LEAF_SIZE, "no_leaves") == "no_leaves") {
            leaves = "small_leaves";
        } else {
            leaves = "large_leaves";
            if (matches(underUnder.mName)) {
                level.setBlock(underPosition, DecorationSupport::withState(under, LEAF_SIZE, "small_leaves"), true);
                level.setBlock(underUnderPosition, DecorationSupport::withState(underUnder, LEAF_SIZE, "no_leaves"),
                               true);
            }
        }
    }

    const bool thick = state.mStates.getString(STALK_THICKNESS, "thin") == "thick" || matches(underUnder.mName);
    const bool stopped = height == MAX_BAMBOO_HEIGHT - 1
                         || (height >= BAMBOO_SLOWDOWN_HEIGHT && RandomTickSystem::nextInt(4) == 0);

    BlockState grown = VanillaBlocks::BAMBOO().toBlockState();
    grown = DecorationSupport::withState(grown, STALK_THICKNESS, thick ? "thick" : "thin");
    grown = DecorationSupport::withState(grown, LEAF_SIZE, leaves);
    grown = DecorationSupport::withState(grown, AGE_BIT, stopped ? 1 : 0);
    level.setBlock(above(position), grown, true);
}
