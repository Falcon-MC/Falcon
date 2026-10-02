#include "Block/Blocks/IcicleBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockQuery.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

#include <string>

FALCON_REGISTER_BLOCK(IcicleBlock, 323);

using namespace BlockQuery;

namespace {
    const char *GROWTH_BASE = "minecraft:packed_ice";
    const int32_t ICICLE_GROWTH_PER_MILLION = 25000;
    const int32_t MAX_LENGTH = 5;

    Vector3i offset(const Vector3i &position, int32_t dy) {
        return Vector3i(position.x, position.y + dy, position.z);
    }
}

bool IcicleBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool IcicleBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    if (isHanging(state))
        return true;

    const Vector3i below = offset(position, -1);
    return level.isSolidAt(below.x, below.y, below.z) || pointsTowards(stateAt(level, below), false);
}

void IcicleBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) owner;

    if (!isHanging(state) || stateAt(level, offset(position, 1)).mName != GROWTH_BASE)
        return;

    if (RandomTickSystem::nextInt(1000000) >= ICICLE_GROWTH_PER_MILLION)
        return;

    Vector3i tip;
    if (!findTip(level, position, true, tip) || position.y - tip.y + 1 >= MAX_LENGTH)
        return;

    grow(level, tip, true);
}
