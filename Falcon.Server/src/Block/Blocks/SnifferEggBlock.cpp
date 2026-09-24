#include "Block/Blocks/SnifferEggBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/EggHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(SnifferEggBlock, 185);

namespace {
    const int32_t REGULAR_HATCH_TICKS = 24000;
    const int32_t BOOSTED_HATCH_TICKS = 12000;
    const int32_t CRACK_STAGES = 3;
    const int32_t HATCH_DELAY_SPREAD = 300;
    const float SOUND_VOLUME = 0.7f;
    const float BABY_SCALE = 0.5f;
    const char *const CRACK_SOUND = "block.sniffer_egg.crack";
    const char *const HATCH_SOUND = "block.sniffer_egg.hatch";
    const char *const SNIFFER = "minecraft:sniffer";

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }
}

bool SnifferEggBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:sniffer_egg";
}

void SnifferEggBlock::scheduleNextCrack(Level &level, const Vector3i &position) {
    if (level.isUpdateScheduled(position))
        return;

    const bool boosted = level.getBlockState(position.x, position.y - 1, position.z).mName == "minecraft:moss_block";
    const int32_t hatchTicks = boosted ? BOOSTED_HATCH_TICKS : REGULAR_HATCH_TICKS;
    level.scheduleUpdate(position, hatchTicks / CRACK_STAGES + RandomTickSystem::nextInt(HATCH_DELAY_SPREAD));
}

void SnifferEggBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                               const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    Block::onPlaced(owner, player, position, state, usedItem, blockFace);

    scheduleNextCrack(owner.getLevelFor(player), position);
}

void SnifferEggBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    Block::onNeighbourChanged(owner, level, position, state);

    if (level.getBlockState(position.x, position.y, position.z).mName == state.mName)
        scheduleNextCrack(level, position);
}

void SnifferEggBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    const Vector3f center = centerOf(position);

    if (EggHelpers::isFullyCracked(state)) {
        owner.playNamedSound(level, HATCH_SOUND, center, SOUND_VOLUME, EggHelpers::soundPitch());
        level.setBlock(position, BlockState("minecraft:air"), true);
        owner.spawnBabyActor(level, SNIFFER, Vector3f(center.x, (float) position.y, center.z), BABY_SCALE);
        return;
    }

    owner.playNamedSound(level, CRACK_SOUND, center, SOUND_VOLUME, EggHelpers::soundPitch());
    level.setBlock(position, EggHelpers::cracked(state), true);
    scheduleNextCrack(level, position);
}
