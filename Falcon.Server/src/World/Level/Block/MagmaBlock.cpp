#include "World/Level/Block/MagmaBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BubbleColumnBlock.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "Network/ServerNetworkHandler.h"

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

// water changing just above asks for a column two ticks later on the second queue; the snow and
// ice melting branch is not ported
void MagmaBlock::onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                        const Vector3i &changed) const {
    (void) state;
    const bool above = changed.x == position.x && changed.z == position.z && changed.y == position.y + 1;
    if (!above || ticks.propertiesAt(changed).material != LiquidMaterial::Water || ticks.hasTickAt(position))
        return;
    ticks.queueRandomTick(position, this, 2);
}

void MagmaBlock::onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
    (void) state;
    BubbleColumnBlock::growAbove(ticks, position);
}
