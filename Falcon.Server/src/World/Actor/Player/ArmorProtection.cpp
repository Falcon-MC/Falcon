#include "World/Actor/Player/ArmorProtection.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Item/BodyArmor.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemEnchantments.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

float ArmorProtection::apply(const ItemStack *armor, size_t count, float amount, const std::string &deathMessageKey,
                             float efficiency) {
    const bool fireDamage = deathMessageKey == "death.attack.lava" || deathMessageKey == "death.attack.onFire"
                            || deathMessageKey == "death.attack.inFire";
    const bool fallDamage = deathMessageKey == "death.fell.accident.generic";
    const bool explosionDamage = deathMessageKey == "death.attack.explosion";
    const bool projectileDamage = deathMessageKey == "death.attack.arrow";
    const bool armorDamage = isReducedByArmor(deathMessageKey);

    int armorPoints = 0;
    int enchantmentProtectionFactor = 0;
    for (size_t slot = 0; slot < count; ++slot) {
        const ItemStack &piece = armor[slot];
        if (piece.isAir() || piece.mDefinition == nullptr)
            continue;

        armorPoints += getPoints(piece);

        enchantmentProtectionFactor += _protectionFactor(ItemEnchantments::getLevel(piece, EnchantmentIds::PROTECTION),
                                                         0.75f);
        if (fireDamage)
            enchantmentProtectionFactor += _protectionFactor(
                    ItemEnchantments::getLevel(piece, EnchantmentIds::FIRE_PROTECTION), 1.25f);
        if (fallDamage)
            enchantmentProtectionFactor += _protectionFactor(
                    ItemEnchantments::getLevel(piece, EnchantmentIds::FEATHER_FALLING), 2.5f);
        if (explosionDamage)
            enchantmentProtectionFactor += _protectionFactor(
                    ItemEnchantments::getLevel(piece, EnchantmentIds::BLAST_PROTECTION), 1.5f);
        if (projectileDamage)
            enchantmentProtectionFactor += _protectionFactor(
                    ItemEnchantments::getLevel(piece, EnchantmentIds::PROJECTILE_PROTECTION), 1.5f);
    }

    const float effectivePoints = (float) armorPoints * efficiency;
    const float effectiveFactor = (float) enchantmentProtectionFactor * efficiency;

    if (armorDamage)
        // vanilla caps armor at 80%: 20 points at 4% each; the clamp holds even for an efficiency above one
        amount *= std::max(0.0f, 1.0f - std::min(0.8f, effectivePoints * 0.04f));

    if (effectiveFactor > 0.0f) {
        const int scaledProtection = std::min(
                (int) std::ceil(std::min(effectiveFactor, 25.0f) * (50.0f + (float) (rand() % 51)) / 100.0f), 20);
        amount *= std::max(0.0f, 1.0f - (float) scaledProtection * 0.04f);
    }

    return amount;
}

bool ArmorProtection::isReducedByArmor(const std::string &deathMessageKey) {
    return deathMessageKey != "death.attack.inFire" && deathMessageKey != "death.attack.drown"
           && deathMessageKey != "death.fell.accident.generic" && deathMessageKey != "death.attack.outOfWorld"
           && deathMessageKey != "death.attack.thorns" && deathMessageKey != "death.attack.suicide";
}

int ArmorProtection::getPoints(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return 0;

    if (const BodyArmorData *body = BodyArmor::find(item))
        return body->mPoints;

    if (const CustomItemDefinition *custom =
                CustomContentRegistry::getInstance().findItem(item.mDefinition->getIdentifier()))
        return custom->mWearableSlot == ArmorSlot::None ? 0 : custom->mProtection;

    const ItemData *data = ItemDataTable::find(item.mDefinition->getIdentifier());
    return data == nullptr ? 0 : data->mArmorPoints;
}

int ArmorProtection::_protectionFactor(int level, float modifier) {
    if (level <= 0)
        return 0;
    return (int) std::floor((6.0f + (float) (level * level)) * modifier / 3.0f);
}
