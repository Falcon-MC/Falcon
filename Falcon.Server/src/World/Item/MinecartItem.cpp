#include "World/Item/MinecartItem.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Vehicle/MinecartActor.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Block/BaseRailBlock.h"
#include "World/Level/Level.h"

FALCON_REGISTER_ITEM(MinecartItem, 100);

namespace {
    // the minecart sits a sixteenth above the rail, half a block higher on a slope
    const float RAIL_HEIGHT = 0.0625f;
    const float SLOPE_HEIGHT = 0.5f;
}

MinecartItem::MinecartItem(const Item &base) : Item(base) {}

bool MinecartItem::matches(const std::string &identifier) {
    return !MinecartActor::actorOfItem(identifier).empty();
}

bool MinecartItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;
    if (item.mDefinition == nullptr || player.isSpectator())
        return false;

    Level &level = owner.getLevelFor(player);
    const BlockState *rail = level.peekBlockPtr(blockPosition.x, blockPosition.y, blockPosition.z);
    if (rail == nullptr || !BaseRailBlock::matches(rail->mName))
        return false;

    const int32_t direction = rail->mStates.getInt("rail_direction", 0);
    const bool slope = direction >= BaseRailBlock::ASCENDING_EAST && direction <= BaseRailBlock::ASCENDING_SOUTH;
    const Vector3f position((float) blockPosition.x + 0.5f,
                            (float) blockPosition.y + RAIL_HEIGHT + (slope ? SLOPE_HEIGHT : 0.0f),
                            (float) blockPosition.z + 0.5f);
    if (owner.spawnActor(level, MinecartActor::actorOfItem(item.mDefinition->getIdentifier()), position) == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}
