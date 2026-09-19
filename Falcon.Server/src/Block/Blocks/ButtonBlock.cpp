#include "Block/Blocks/ButtonBlock.h"

#include "Block/BlockSupport.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *BUTTON_SUFFIX = "_button";
    const size_t BUTTON_SUFFIX_LENGTH = 7;
}

bool ButtonBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const
{
    const Vector3i supportPosition = BlockSupport::supportOf(position, blockFace);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);
    return BlockSupport::isAttachable(support, blockFace);
}

bool ButtonBlock::matches(const std::string &identifier)
{
    if (identifier.size() < BUTTON_SUFFIX_LENGTH)
        return false;

    return identifier.compare(identifier.size() - BUTTON_SUFFIX_LENGTH, BUTTON_SUFFIX_LENGTH,
                              BUTTON_SUFFIX) == 0;
}

bool ButtonBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state) const
{
    RedstoneSystem::onButtonActivated(owner, owner.getLevelFor(player), position, state);
    return true;
}
