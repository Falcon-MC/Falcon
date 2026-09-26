#include "Item/Items/EnchantedGoldenAppleItem.h"

#include "Item/ItemClassRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_ITEM(EnchantedGoldenAppleItem, 100);

EnchantedGoldenAppleItem::EnchantedGoldenAppleItem(const Item &base) : Item(base) {
}

bool EnchantedGoldenAppleItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void EnchantedGoldenAppleItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player,
                                          const ItemStack &item) const {
    (void) item;
    owner.addEffect(player, MobEffectId::Regeneration, REGENERATION_AMPLIFIER, REGENERATION_DURATION_TICKS);
    owner.addEffect(player, MobEffectId::Absorption, ABSORPTION_AMPLIFIER, ABSORPTION_DURATION_TICKS);
    owner.addEffect(player, MobEffectId::Resistance, 0, RESISTANCE_DURATION_TICKS);
    owner.addEffect(player, MobEffectId::FireResistance, 0, FIRE_RESISTANCE_DURATION_TICKS);
}
