#include "World/Level/Block/ComposterBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(ComposterBlock, 100);

bool ComposterBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::COMPOSTER;
}

// ComposterBlock::getComparatorSignal (sub_14771A230, 26.60): the fill level as is
int ComposterBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return std::clamp(state.mStates.getInt("composter_fill_level", 0), 0, 8);
}
