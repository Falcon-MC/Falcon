#include "World/Item/GlassBottleItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(GlassBottleItem, 100);

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/BucketItem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

namespace {
    const char *const WATER_BLOCK = "minecraft:water";
    const char *const WATER_BOTTLE = "minecraft:potion";
    const char *const FILL_SOUND = "bottle.fill";
}

GlassBottleItem::GlassBottleItem(const Item &base) : Item(base) {}

bool GlassBottleItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool GlassBottleItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                   const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    Level &level = owner.getLevelFor(player);
    if (level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z).mName != WATER_BLOCK)
        return false;

    const Vector3f center((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                          (float) blockPosition.z + 0.5f);
    owner.playLevelSound(level, FILL_SOUND, center);
    return BucketItem::applyResult(owner, player, item, WATER_BOTTLE);
}
