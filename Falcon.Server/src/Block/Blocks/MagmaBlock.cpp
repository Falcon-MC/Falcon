#include "Block/Blocks/MagmaBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(MagmaBlock, 136);

namespace {
    const float HOT_FLOOR_DAMAGE = 1.0f;
}

bool MagmaBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:magma";
}

void MagmaBlock::onStepOn(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                          const BlockState &state) const {
    (void) position;
    (void) state;

    if (player.hasEffect(MobEffectId::FireResistance) || player.getFlags().get(ActorFlag::Sneaking))
        return;

    const ItemStack &boots = player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    if (ItemEnchantments::getLevel(boots, EnchantmentIds::FROST_WALKER) > 0)
        return;

    owner.applyDamage(player, HOT_FLOOR_DAMAGE, "death.attack.hotFloor", {player.getName()});
}
