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

void MagmaBlock::onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                          const BlockState &state) const {
    (void) position;
    (void) state;

    if (actor.isFireImmune() || actor.hasEffect(MobEffectId::FireResistance)
        || actor.getFlags().get(ActorFlag::Sneaking))
        return;

    if (const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor)) {
        const ItemStack &boots = player->getInventory().getArmor(PlayerInventory::ARMOR_FEET);
        if (ItemEnchantments::getLevel(boots, EnchantmentIds::FROST_WALKER) > 0)
            return;
    }

    owner.hurtActor(actor, HOT_FLOOR_DAMAGE, "death.attack.hotFloor");
}
