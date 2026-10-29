#include "World/Item/SpawnEggItem.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include "World/Item/ItemClassRegistry.h"

#include <cmath>
#include <string>

namespace {
    const std::string SPAWN_EGG_SUFFIX = "_spawn_egg";
    const char *const DISPLAY_TAG = "display";
    const char *const NAME_TAG = "Name";
    const float PLAYER_HALF_WIDTH = 0.3f;
    const float FACING_OFFSET_DEGREES = 90.0f;
    const float RADIANS_TO_DEGREES = 57.295776f;
    const float MIN_FACING_DISTANCE_SQUARED = 0.00000011920929f;

    std::string resolveSpawnedActor(const std::string &identifier) {
        if (identifier == "minecraft:villager")
            return "minecraft:villager_v2";
        if (identifier == "minecraft:zombie_villager")
            return "minecraft:zombie_villager_v2";
        if (identifier == "minecraft:evoker")
            return "minecraft:evocation_illager";
        if (identifier == "minecraft:tropical_fish")
            return "minecraft:tropicalfish";
        return identifier;
    }

    // The client turns the spawned mob towards the player, measured from the player's box corner.
    float yawTowards(const ServerPlayer &player, const Vector3f &spawnPosition) {
        const Vector3f &position = player.getPosition();
        const float dx = position.x - PLAYER_HALF_WIDTH - spawnPosition.x;
        const float dz = position.z - PLAYER_HALF_WIDTH - spawnPosition.z;
        if (dx * dx + dz * dz <= MIN_FACING_DISTANCE_SQUARED)
            return 0.0f;
        return std::atan2(dz, dx) * RADIANS_TO_DEGREES - FACING_OFFSET_DEGREES;
    }

    std::string customNameOf(const ItemStack &item) {
        const Tag *display = item.mTag.get(DISPLAY_TAG);
        if (display == nullptr || !display->isCompound())
            return std::string();
        return display->getString(NAME_TAG, std::string());
    }
}

FALCON_REGISTER_ITEM_CUSTOM(SpawnEggItem, 90,
                            [](const std::string &identifier) {
                                return identifier.size() > SPAWN_EGG_SUFFIX.size()
                                       && identifier.compare(identifier.size() - SPAWN_EGG_SUFFIX.size(),
                                                             SPAWN_EGG_SUFFIX.size(), SPAWN_EGG_SUFFIX) == 0;
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<SpawnEggItem>(item);
                            });

SpawnEggItem::SpawnEggItem(const Item &base) : Item(base) {}

bool SpawnEggItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face,
                                const Vector3f &clickPosition) const {
    (void) clickPosition;

    if (item.mDefinition == nullptr)
        return false;

    const std::string &identifier = item.mDefinition->getIdentifier();
    if (identifier.size() <= SPAWN_EGG_SUFFIX.size() ||
        identifier.compare(identifier.size() - SPAWN_EGG_SUFFIX.size(), SPAWN_EGG_SUFFIX.size(),
                           SPAWN_EGG_SUFFIX) != 0)
        return false;

    const std::string entityIdentifier =
            resolveSpawnedActor(identifier.substr(0, identifier.size() - SPAWN_EGG_SUFFIX.size()));

    Vector3f spawnPosition((float) blockPosition.x + 0.5f, (float) blockPosition.y,
                           (float) blockPosition.z + 0.5f);
    switch (face) {
        case 0:
            spawnPosition.y -= 1.0f;
            break;
        case 1:
            spawnPosition.y += 1.0f;
            break;
        case 2:
            spawnPosition.z -= 1.0f;
            break;
        case 3:
            spawnPosition.z += 1.0f;
            break;
        case 4:
            spawnPosition.x -= 1.0f;
            break;
        case 5:
            spawnPosition.x += 1.0f;
            break;
        default:
            spawnPosition.y += 1.0f;
            break;
    }

    const float yaw = yawTowards(player, spawnPosition);
    const std::string customName = customNameOf(item);
    ServerActor *spawned = owner.spawnActor(owner.getLevelFor(player), entityIdentifier, spawnPosition,
                                            [&](ServerActor &actor) {
                                                actor.setRotation(Vector3f(0.0f, yaw, yaw));
                                                if (!customName.empty())
                                                    actor.setNameTag(customName);
                                            });
    if (spawned == nullptr)
        return false;

    if (!customName.empty())
        owner.sendActorNameTag(*spawned);

    player.consumeOneHeldItem();
    return true;
}
