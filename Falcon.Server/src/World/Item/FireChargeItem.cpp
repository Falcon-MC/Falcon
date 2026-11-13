#include "World/Item/FireChargeItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(
        FireChargeItem, 41, [](const std::string &identifier) { return identifier == "minecraft:fire_charge"; },
        [](const Item &item) -> std::unique_ptr<Item> { return std::make_unique<FireChargeItem>(item); });

#include "World/Level/Block/Systems/FireSystem.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/FireStarterHelpers.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Level/Block/DispenseSource.h"

#include <random>

using namespace FireStarterHelpers;

namespace {
    const char *const SMALL_FIREBALL = "minecraft:small_fireball";
    // LevelEvent 1008, the blaze shoot sound
    const int32_t LEVEL_EVENT_FIREBALL = 1008;
    const float DISPENSE_SPREAD = 0.05f;
}

FireChargeItem::FireChargeItem(const Item &base) : Item(base) {}

bool FireChargeItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                  const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) item;
    (void) clickPosition;

    if (player.isAdventure())
        return false;

    Level &level = owner.getLevelFor(player);
    const Vector3i placement = relativeToFace(blockPosition, face);
    if (!canIgniteAgainst(level, blockPosition, placement))
        return false;

    if (!FireSystem::ignite(owner, level, placement, isObsidian(level, blockPosition)))
        return false;

    owner.broadcastLevelEvent(level, LevelEventPacket::SoundGhastFireball, centerOf(placement), 0);
    player.consumeOneHeldItem();
    return true;
}

// FireChargeItem::dispense (sub_147BA2860, 26.60): the facing axis with a 0.05 gaussian on each axis, then
// the small fireball's own shot
bool FireChargeItem::dispense(const DispenseSource &source) const {
    static std::mt19937 generator{std::random_device{}()};
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    const Vector3f facing = source.getFacingVector();
    const float x = gaussian(generator) * DISPENSE_SPREAD + facing.x;
    const float y = gaussian(generator) * DISPENSE_SPREAD + facing.y;
    const float z = gaussian(generator) * DISPENSE_SPREAD + facing.z;
    if (source.mOwner.spawnDispensedProjectile(source.mLevel, SMALL_FIREBALL, source.mPosition, Vector3f(x, y, z))
        == nullptr)
        return false;

    source.removeOne();
    source.broadcastShootEvent(LEVEL_EVENT_FIREBALL);
    return true;
}
