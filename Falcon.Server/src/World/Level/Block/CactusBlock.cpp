#include "World/Level/Block/CactusBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/PlantGrowthHelpers.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(CactusBlock, 311);

using namespace PlantGrowthHelpers;

namespace {
    const int32_t MAX_CACTUS_HEIGHT = 3;

    bool isHorizontalNeighbourhoodClear(Level &level, const Vector3i &position) {
        for (int32_t face: HORIZONTAL_FACES) {
            if (!isAirAt(level, side(position, face)))
                return false;
        }
        return true;
    }
}

bool CactusBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cactus";
}

void CactusBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) owner;

    if (matches(stateAt(level, below(position)).mName))
        return;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age < MAX_AGE) {
        level.setBlock(position, DecorationSupport::withState(state, AGE, age + 1), false);
        return;
    }

    for (int32_t offset = 1; offset <= MAX_CACTUS_HEIGHT; ++offset) {
        const Vector3i target(position.x, position.y + offset, position.z);
        const BlockState targetState = stateAt(level, target);
        if (matches(targetState.mName))
            continue;

        if (!DecorationSupport::isAir(targetState) || !isInRange(level, target))
            return;

        const int32_t roll = RandomTickSystem::nextInt(101);
        const bool flower = (offset < 2 && roll < 10) || (offset == MAX_CACTUS_HEIGHT && roll <= 25);
        if ((flower && !isHorizontalNeighbourhoodClear(level, target)) || (!flower && offset == MAX_CACTUS_HEIGHT)) {
            resetAge(level, position, state);
            return;
        }

        const BlockState grown = flower ? VanillaBlocks::CACTUS_FLOWER().toBlockState()
                                        : VanillaBlocks::CACTUS().toBlockState();
        BlockChangeSystem::change(level, target, grown, BlockChangeCause::Grow, true);
        break;
    }

    resetAge(level, position, state);
}
