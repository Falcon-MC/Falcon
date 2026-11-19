#include "World/Item/PaintingItem.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Misc/PaintingActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Level.h"

#include <algorithm>

FALCON_REGISTER_ITEM(PaintingItem, 110);

PaintingItem::PaintingItem(const Item &base) : Item(base) {}

bool PaintingItem::matches(const std::string &identifier) {
    return identifier == PaintingActor::IDENTIFIER;
}

bool PaintingItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face, const Vector3f &) const {
    if (player.isAdventure()) {
        const auto *support = owner.getLevelFor(player).peekBlockPtr(blockPosition.x, blockPosition.y, blockPosition.z);
        if (support == nullptr || std::find(item.mCanPlace.begin(), item.mCanPlace.end(), support->mName)
                                          == item.mCanPlace.end())
            return false;
    }

    if (!PaintingActor::place(owner, player, blockPosition, face))
        return false;

    player.consumeOneHeldItem();
    return true;
}
