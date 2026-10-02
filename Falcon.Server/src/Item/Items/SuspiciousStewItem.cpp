#include "Item/Items/SuspiciousStewItem.h"

#include "Actor/MobEffect.h"
#include "Item/ItemClassRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_ITEM(SuspiciousStewItem, 100);

namespace {
    struct StewEffect {
        MobEffectId mId;
        int32_t mDurationTicks;
    };

    const int32_t TICKS_PER_SECOND = 20;

    const StewEffect STEW_EFFECTS[] = {
            {MobEffectId::NightVision, 5 * TICKS_PER_SECOND},
            {MobEffectId::JumpBoost, 5 * TICKS_PER_SECOND},
            {MobEffectId::Weakness, 7 * TICKS_PER_SECOND},
            {MobEffectId::Blindness, 7 * TICKS_PER_SECOND},
            {MobEffectId::Poison, 11 * TICKS_PER_SECOND},
            {MobEffectId::Saturation, 6},
            {MobEffectId::Saturation, 6},
            {MobEffectId::FireResistance, 3 * TICKS_PER_SECOND},
            {MobEffectId::Regeneration, 7 * TICKS_PER_SECOND},
            {MobEffectId::Wither, 7 * TICKS_PER_SECOND},
            {MobEffectId::NightVision, 5 * TICKS_PER_SECOND},
            {MobEffectId::Blindness, 7 * TICKS_PER_SECOND},
            {MobEffectId::Nausea, 7 * TICKS_PER_SECOND},
    };
}

SuspiciousStewItem::SuspiciousStewItem(const Item &base) : Item(base) {
}

bool SuspiciousStewItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void SuspiciousStewItem::onConsumed(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    const int32_t count = (int32_t) (sizeof(STEW_EFFECTS) / sizeof(STEW_EFFECTS[0]));
    if (item.mDamage < 0 || item.mDamage >= count)
        return;

    const StewEffect &effect = STEW_EFFECTS[item.mDamage];
    owner.addEffect(player, effect.mId, 0, effect.mDurationTicks);
}
