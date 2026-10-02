#include "Item/Items/SpawnEggItem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include "Item/ItemClassRegistry.h"

#include <string>

namespace {
    const std::string SPAWN_EGG_SUFFIX = "_spawn_egg";

    std::string resolveSpawnedActor(const std::string &identifier) {
        if (identifier == "minecraft:villager")
            return "minecraft:villager_v2";
        if (identifier == "minecraft:zombie_villager")
            return "minecraft:zombie_villager_v2";
        return identifier;
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

    if (owner.spawnActor(owner.getLevelFor(player), entityIdentifier, spawnPosition) == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}
