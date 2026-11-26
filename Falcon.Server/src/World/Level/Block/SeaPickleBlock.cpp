#include "World/Level/Block/SeaPickleBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SeaPickleBlock, 100);

#include "Network/Handler/BlockActionHandler.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/CoralBlock.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

namespace {
    const char *const STATE_CLUSTER = "cluster_count";
    const char *const STATE_DEAD = "dead_bit";
    const int MAX_CLUSTER = 3;

    bool onLiveCoralBlock(Level &level, const Vector3i &position) {
        const BlockState *below = level.peekBlockPtr(position.x, position.y - 1, position.z);
        return below != nullptr && CoralBlock::isLiveCoralBlock(below->mName);
    }

    // a pickle placed in water keeps that water in its second layer
    void placeInWater(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &water,
                      int cluster) {
        BlockState pickle = VanillaBlocks::SEA_PICKLE().toBlockState();
        pickle.mStates.putInt(STATE_CLUSTER, cluster);
        pickle.mStates.putBool(STATE_DEAD, false);
        if (!BlockChangeSystem::change(level, position, pickle, BlockChangeCause::Grow, true))
            return;
        level.setBlockStateAtLayer(position.x, position.y, position.z, 1, water);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, water, 1);
    }
}

bool SeaPickleBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SEA_PICKLE;
}

// a living pickle on a live coral block: a 1, 3, 5, 3, 1 wide diamond around it, one block down to its own level,
// gets a 1 in 6 pickle of 1 to 4 on each watered coral block, and the clicked pickle becomes four
bool SeaPickleBlock::onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    if (state.mStates.getBool(STATE_DEAD, false) || !onLiveCoralBlock(level, position))
        return false;

    for (int32_t column = 0; column < 5; ++column) {
        const int32_t halfWidth = column < 3 ? column : 4 - column;
        for (int32_t dz = -halfWidth; dz <= halfWidth; ++dz) {
            for (int32_t dy = -1; dy <= 0; ++dy) {
                const Vector3i target(position.x - 2 + column, position.y + dy, position.z + dz);
                if (target == position || RandomTickSystem::nextInt(6) != 0)
                    continue;
                const BlockState *at = level.peekBlockPtr(target.x, target.y, target.z);
                if (at == nullptr || !LiquidView(*at).isWater() || !onLiveCoralBlock(level, target))
                    continue;
                placeInWater(owner, level, target, *at, RandomTickSystem::nextInt(MAX_CLUSTER + 1));
            }
        }
    }

    Tag states = state.mStates;
    states.putInt(STATE_CLUSTER, MAX_CLUSTER);
    BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow, true);
    return true;
}
