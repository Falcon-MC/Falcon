#include "World/Item/BoatItem.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Vehicle/BoatActor.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Level.h"

#include <cmath>

FALCON_REGISTER_ITEM(BoatItem, 100);

namespace {
    // the boat's forward axis is a quarter turn from the player's: it faces where the player looks
    const float FACING_TURN_DEGREES = 90.0f;

    bool isChestItem(const std::string &identifier) {
        return identifier.find("_chest_") != std::string::npos || identifier == "minecraft:chest_boat";
    }
}

BoatItem::BoatItem(const Item &base) : Item(base) {}

bool BoatItem::matches(const std::string &identifier) {
    return BoatActor::variantOfItem(identifier) >= 0;
}

bool BoatItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                            const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    if (item.mDefinition == nullptr || player.isSpectator())
        return false;
    if (!std::isfinite(clickPosition.x) || !std::isfinite(clickPosition.y) || !std::isfinite(clickPosition.z)
        || clickPosition.x < 0.0f || clickPosition.x > 1.0f || clickPosition.y < 0.0f || clickPosition.y > 1.0f
        || clickPosition.z < 0.0f || clickPosition.z > 1.0f)
        return false;

    const std::string &identifier = item.mDefinition->getIdentifier();
    const int32_t variant = BoatActor::variantOfItem(identifier);
    if (variant < 0)
        return false;

    // the boat lands on the point the player's ray hit, water surface included
    const Vector3f position((float) blockPosition.x + clickPosition.x, (float) blockPosition.y + clickPosition.y,
                            (float) blockPosition.z + clickPosition.z);
    const float half = BoatActor::WIDTH * 0.5f;
    const AxisAlignedBB box(position.x - half, position.y, position.z - half, position.x + half,
                            position.y + BoatActor::HEIGHT, position.z + half);
    Level &level = owner.getLevelFor(player);
    for (const AxisAlignedBB &shape: level.getCollisionBoxes(box)) {
        if (shape.intersectsWith(box))
            return false;
    }

    const float yaw = player.getRotation().y + FACING_TURN_DEGREES;
    const std::string actor = isChestItem(identifier) ? BoatActor::CHEST_BOAT : BoatActor::BOAT;
    ServerActor *spawned = owner.spawnActor(level, actor, position, [&](ServerActor &boat) {
        boat.setRotation(Vector3f(0.0f, yaw, yaw));
        if (auto *placed = dynamic_cast<BoatActor *>(&boat))
            placed->setVariant(variant);
    });
    if (spawned == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}
