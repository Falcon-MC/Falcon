#include "World/Level/Block/CakeBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(CakeBlock, 100);

bool CakeBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::CAKE;
}

// CakeBlock::getComparatorSignal: two less per slice eaten
int CakeBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return 14 - 2 * std::clamp(state.mStates.getInt("bite_counter", 0), 0, 6);
}
