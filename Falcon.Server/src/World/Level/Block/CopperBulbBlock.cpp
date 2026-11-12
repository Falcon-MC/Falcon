#include "World/Level/Block/CopperBulbBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(CopperBulbBlock, 100);

namespace {
    const char *STATE_POWERED = "powered_bit";
    const char *STATE_LIT = "lit";
    // LevelSoundEvent 490 and 491, the bulb sounds 0x1EA | was lit
    const char *SOUND_TURN_ON = "block.copper_bulb.turn_on";
    const char *SOUND_TURN_OFF = "block.copper_bulb.turn_off";
}

bool CopperBulbBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {BlockNames::COPPER_BULB, BlockNames::EXPOSED_COPPER_BULB,
                                                   BlockNames::WEATHERED_COPPER_BULB, BlockNames::OXIDIZED_COPPER_BULB,
                                                   BlockNames::WAXED_COPPER_BULB, BlockNames::WAXED_EXPOSED_COPPER_BULB,
                                                   BlockNames::WAXED_WEATHERED_COPPER_BULB,
                                                   BlockNames::WAXED_OXIDIZED_COPPER_BULB});
}

// CopperBulbBlock redstone handler (sub_1477220B0, 26.60): a change of power waits one tick
void CopperBulbBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    if ((state.mStates.getByte(STATE_POWERED, 0) != 0) != powered)
        level.scheduleUpdate(position, 1);
}

// CopperBulbBlock tick (sub_147721890, 26.60): powered_bit follows the power; each rising edge flips lit
void CopperBulbBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    const bool wasPowered = state.mStates.getByte(STATE_POWERED, 0) != 0;
    if (powered == wasPowered)
        return;

    Tag states = state.mStates;
    states.putByte(STATE_POWERED, powered ? 1 : 0);
    if (!wasPowered) {
        const bool wasLit = state.mStates.getByte(STATE_LIT, 0) != 0;
        states.putByte(STATE_LIT, wasLit ? 0 : 1);
        // the sound sits on the block corner, not its center
        owner.playLevelSound(level, wasLit ? SOUND_TURN_OFF : SOUND_TURN_ON,
                             Vector3f((float) position.x, (float) position.y, (float) position.z));
    }

    level.setBlock(position, BlockState(state.mName, states), true);
}

// CopperBulbBlock::getComparatorSignal (sub_147722290, 26.60)
int CopperBulbBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return state.mStates.getByte(STATE_LIT, 0) != 0 ? 15 : 0;
}
