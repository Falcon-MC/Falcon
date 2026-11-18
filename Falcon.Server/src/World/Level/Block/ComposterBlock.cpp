#include "World/Level/Block/ComposterBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>

namespace {
    const char *const FILL_LEVEL = "composter_fill_level";
    const int32_t READY_LEVEL = 8;
    const char *const EMPTY_SOUND = "block.composter.empty";
}

FALCON_REGISTER_BLOCK(ComposterBlock, 100);

bool ComposterBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::COMPOSTER;
}

// ComposterBlock::getComparatorSignal: the fill level as is
int ComposterBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return std::clamp(state.mStates.getInt(FILL_LEVEL, 0), 0, READY_LEVEL);
}

bool ComposterBlock::takeOutput(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr || !matches(state->mName) || state->mStates.getInt(FILL_LEVEL, 0) != READY_LEVEL)
        return false;

    BlockState emptied = *state;
    emptied.mStates.putInt(FILL_LEVEL, 0);
    level.setBlock(position, emptied, true);
    owner.playLevelSound(level, EMPTY_SOUND,
                         Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f));
    return true;
}
