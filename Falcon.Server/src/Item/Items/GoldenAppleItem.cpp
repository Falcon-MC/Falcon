#include "Item/Items/GoldenAppleItem.h"

#include "Item/ItemClassRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_ITEM(GoldenAppleItem, 100);

GoldenAppleItem::GoldenAppleItem(const Item &base) : Item(base) {
}

bool GoldenAppleItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void GoldenAppleItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;
    owner.addEffect(player, MobEffectId::Absorption, 0, ABSORPTION_DURATION_TICKS);
    owner.addEffect(player, MobEffectId::Regeneration, REGENERATION_AMPLIFIER, REGENERATION_DURATION_TICKS);
}
