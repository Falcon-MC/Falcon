#include "World/Level/Block/AnvilBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(AnvilBlock, 10);

#include "World/Containers/Managers/Models/AnvilContainerManagerModel.h"
#include "World/Level/Block/Systems/FallingBlockSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/BlockNames.h"

#include <random>

namespace {
    const float DAMAGE_CHANCE = 0.12f;

    // LevelEvents: 1021 after every use, 1020 when the anvil breaks
    const int32_t LEVEL_EVENT_ANVIL_USED = 1021;
    const int32_t LEVEL_EVENT_ANVIL_BROKEN = 1020;

    float nextFloat() {
        static std::mt19937 random(std::random_device{}());
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(random);
    }
}

AnvilBlock::AnvilBlock() : Block(BlockTypeIds::ANVIL, "minecraft:anvil", "Anvil") {}

AnvilBlock::AnvilBlock(const Block &block) : Block(block) {}

bool AnvilBlock::matches(const BlockState &state) {
    return matches(state.mName);
}

bool AnvilBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:anvil" || identifier == "minecraft:chipped_anvil"
           || identifier == "minecraft:damaged_anvil";
}

bool AnvilBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state) const {
    (void) state;

    AnvilContainerManagerModel model;
    model.open(owner, player, position);
    return true;
}

void AnvilBlock::onUsed(ServerNetworkHandler &owner, Level &level, const Vector3i &position, bool creative) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    if (!matches(state))
        return;

    const Vector3f eventPosition((float) position.x, (float) position.y, (float) position.z);
    if (creative || nextFloat() >= DAMAGE_CHANCE) {
        owner.broadcastLevelEvent(level, LEVEL_EVENT_ANVIL_USED, eventPosition, 0);
        return;
    }

    const std::string next = FallingBlockSystem::getNextAnvilDamage(state.mName);
    if (next.empty()) {
        level.setBlock(position, BlockState(BlockNames::AIR), true);
        owner.broadcastLevelEvent(level, LEVEL_EVENT_ANVIL_BROKEN, eventPosition, 0);
        return;
    }

    level.setBlock(position, BlockState(next, state.mStates), true);
    owner.broadcastLevelEvent(level, LEVEL_EVENT_ANVIL_USED, eventPosition, 0);
}
