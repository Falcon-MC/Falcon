#include "World/Actor/Movement/Systems/PlayerMovementAdmissionSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/EnchantmentData.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "Network/ServerNetworkHandler.h"

using namespace MovementPhysics;

bool PlayerMovementAdmissionSystem::prepare(PlayerMovementContext &context) {
    auto &state = context.state;
    const auto &input = context.input;

    const ItemStack &boots = context.player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    state.start = context.player.hasSimulatedPosition() ? context.player.getSimulatedPosition() : context.player.getPosition();
    const Block *soulSupport = blockAt(context.level, state.start.x, state.start.y - SUPPORT_PROBE_DEPTH, state.start.z);
    // BDS keeps the modifier over air while jumping, and refreshes it on the next non-air block.
    if (soulSupport != nullptr && soulSupport->getTypeId() != BlockTypeIds::AIR) {
        const int enchantmentLevel = ItemEnchantments::getLevel(boots, EnchantmentIds::SOUL_SPEED);
        const float bonus = enchantmentLevel > 0 && soulSupport->getBehavior().supportsSoulSpeed()
                            ? 0.03f + 0.0105f * (float) enchantmentLevel : 0.0f;
        context.player.getEffects().setMovementSpeedBonus(bonus);
    }
    if (!context.owner.getProperties().getSimulation()) {
        context.player.clearMovementSimulation();
        return false;
    }

    // Only server state decides to skip: a server teleport keeps a movement change pending until the client
    // acknowledges it, and riding is the server's own state. The client's HandleTeleport and vehicle input flags are
    // not trusted, or a single bit would let it place itself anywhere.
    const bool skip = context.player.isRiding() || context.player.isSleeping()
                      || context.player.skipsForPendingMovementChange();
    if (skip || !chunkLoaded(context.level, state.start)
        || !chunkLoaded(context.level, *context.prediction.feetPosition)) {
        context.player.clearMovementSimulation();
        return false;
    }

    return true;
}
