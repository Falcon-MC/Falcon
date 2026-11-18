#include "World/Level/Block/SmallDripleafBlock.h"

#include "World/Level/Block/BigDripleafBlock.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/DoubleBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(SmallDripleafBlock, 100);

namespace {
    const char *const STATE_DIRECTION = "minecraft:cardinal_direction";
}

bool SmallDripleafBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SMALL_DRIPLEAF_BLOCK;
}

// shears only: Silk Touch does not keep it
bool SmallDripleafBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                  std::vector<BlockDrop> &drops) const {
    (void) fortuneLevel;
    if (isShears(tool))
        drops.push_back({state.mName, 1});
    return true;
}

// the upper half passes the bone meal down; the lower half becomes the bottom of a big dripleaf
bool SmallDripleafBlock::onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    if (DoubleBlock::isUpperHalf(state)) {
        const Vector3i lower(position.x, position.y - 1, position.z);
        const BlockState *below = level.peekBlockPtr(lower.x, lower.y, lower.z);
        const Block *block = below == nullptr ? nullptr : VanillaBlocks::fromState(*below);
        return block != nullptr && block->onFertilized(owner, level, lower, *below);
    }

    const Vector3i upper(position.x, position.y + 1, position.z);
    const BlockState *above = level.peekBlockPtr(upper.x, upper.y, upper.z);
    const bool hasUpper = above != nullptr && above->mName == BlockNames::SMALL_DRIPLEAF_BLOCK;
    return BigDripleafBlock::growFromSmall(owner, level, position, state.mStates.getString(STATE_DIRECTION, "south"),
                                           hasUpper ? &upper : nullptr);
}
