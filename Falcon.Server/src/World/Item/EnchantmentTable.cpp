#include "World/Item/EnchantmentData.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
    using namespace EnchantmentTargets;
    namespace G = EnchantmentGroups;

    const EnchantmentData ENCHANTMENTS[] = {
            {EnchantmentIds::PROTECTION, "protection", "Protection", 4, EnchantmentRarity::Common,
                    ARMOR, NONE, G::PROTECTION},
            {EnchantmentIds::FIRE_PROTECTION, "fire_protection", "Fire Protection", 4, EnchantmentRarity::Uncommon,
                    ARMOR, NONE, G::PROTECTION},
            {EnchantmentIds::FEATHER_FALLING, "feather_falling", "Feather Falling", 4, EnchantmentRarity::Uncommon,
                    BOOTS, NONE, G::NONE},
            {EnchantmentIds::BLAST_PROTECTION, "blast_protection", "Blast Protection", 4, EnchantmentRarity::Rare,
                    ARMOR, NONE, G::PROTECTION},
            {EnchantmentIds::PROJECTILE_PROTECTION, "projectile_protection", "Projectile Protection", 4,
                    EnchantmentRarity::Uncommon, ARMOR, NONE, G::PROTECTION},
            {EnchantmentIds::THORNS, "thorns", "Thorns", 3, EnchantmentRarity::Mythic,
                    CHESTPLATE, HELMET | LEGGINGS | BOOTS, G::NONE},
            {EnchantmentIds::RESPIRATION, "respiration", "Respiration", 3, EnchantmentRarity::Rare,
                    HELMET, NONE, G::NONE},
            {EnchantmentIds::DEPTH_STRIDER, "depth_strider", "Depth Strider", 3, EnchantmentRarity::Rare,
                    BOOTS, NONE, G::BOOTS_MOVEMENT},
            {EnchantmentIds::AQUA_AFFINITY, "aqua_affinity", "Aqua Affinity", 1, EnchantmentRarity::Rare,
                    HELMET, NONE, G::NONE},
            {EnchantmentIds::SHARPNESS, "sharpness", "Sharpness", 5, EnchantmentRarity::Common,
                    SWORD | AXE, SPEAR, G::WEAPON_DAMAGE},
            {EnchantmentIds::SMITE, "smite", "Smite", 5, EnchantmentRarity::Uncommon,
                    SWORD | AXE, SPEAR, G::WEAPON_DAMAGE},
            {EnchantmentIds::BANE_OF_ARTHROPODS, "bane_of_arthropods", "Bane of Arthropods", 5,
                    EnchantmentRarity::Uncommon, SWORD | AXE, SPEAR, G::WEAPON_DAMAGE},
            {EnchantmentIds::KNOCKBACK, "knockback", "Knockback", 2, EnchantmentRarity::Uncommon,
                    SWORD, NONE, G::NONE},
            {EnchantmentIds::FIRE_ASPECT, "fire_aspect", "Fire Aspect", 2, EnchantmentRarity::Rare,
                    SWORD | MACE, NONE, G::NONE},
            {EnchantmentIds::LOOTING, "looting", "Looting", 3, EnchantmentRarity::Rare,
                    SWORD, MACE, G::NONE},
            {EnchantmentIds::EFFICIENCY, "efficiency", "Efficiency", 5, EnchantmentRarity::Common,
                    BLOCK_TOOLS, SHEARS, G::NONE},
            {EnchantmentIds::SILK_TOUCH, "silk_touch", "Silk Touch", 1, EnchantmentRarity::Mythic,
                    BLOCK_TOOLS, SHEARS, G::BLOCK_DROPS},
            {EnchantmentIds::UNBREAKING, "unbreaking", "Unbreaking", 3, EnchantmentRarity::Uncommon,
                    ARMOR | WEAPONS | FISHING_ROD, DURABLE_MISC, G::NONE},
            {EnchantmentIds::FORTUNE, "fortune", "Fortune", 3, EnchantmentRarity::Rare,
                    BLOCK_TOOLS, NONE, G::BLOCK_DROPS},
            {EnchantmentIds::POWER, "power", "Power", 5, EnchantmentRarity::Common,
                    BOW, NONE, G::NONE},
            {EnchantmentIds::PUNCH, "punch", "Punch", 2, EnchantmentRarity::Rare,
                    BOW, NONE, G::NONE},
            {EnchantmentIds::FLAME, "flame", "Flame", 1, EnchantmentRarity::Rare,
                    BOW, NONE, G::NONE},
            {EnchantmentIds::INFINITY_ENCHANTMENT, "infinity", "Infinity", 1, EnchantmentRarity::Mythic,
                    BOW, NONE, G::BOW_INFINITE},
            {EnchantmentIds::LUCK_OF_THE_SEA, "luck_of_the_sea", "Luck of the Sea", 3, EnchantmentRarity::Rare,
                    FISHING_ROD, NONE, G::NONE},
            {EnchantmentIds::LURE, "lure", "Lure", 3, EnchantmentRarity::Rare,
                    FISHING_ROD, NONE, G::NONE},
            {EnchantmentIds::FROST_WALKER, "frost_walker", "Frost Walker", 2, EnchantmentRarity::Rare,
                    NONE, BOOTS, G::BOOTS_MOVEMENT},
            {EnchantmentIds::MENDING, "mending", "Mending", 1, EnchantmentRarity::Rare,
                    NONE, ARMOR | WEAPONS | FISHING_ROD | DURABLE_MISC | COMPASS | BOOK, G::BOW_INFINITE},
            {EnchantmentIds::BINDING, "binding", "Curse of Binding", 1, EnchantmentRarity::Mythic,
                    NONE, ARMOR | ELYTRA | MASK | BOOK, G::NONE},
            {EnchantmentIds::VANISHING, "vanishing", "Curse of Vanishing", 1, EnchantmentRarity::Mythic,
                    NONE, ALL, G::NONE},
            {EnchantmentIds::IMPALING, "impaling", "Impaling", 5, EnchantmentRarity::Rare,
                    TRIDENT, NONE, G::NONE},
            {EnchantmentIds::RIPTIDE, "riptide", "Riptide", 3, EnchantmentRarity::Rare,
                    TRIDENT, NONE, G::TRIDENT_PROPULSION},
            {EnchantmentIds::LOYALTY, "loyalty", "Loyalty", 3, EnchantmentRarity::Uncommon,
                    TRIDENT, NONE, G::TRIDENT_PROPULSION},
            {EnchantmentIds::CHANNELING, "channeling", "Channeling", 1, EnchantmentRarity::Mythic,
                    TRIDENT, NONE, G::TRIDENT_PROPULSION},
            {EnchantmentIds::MULTISHOT, "multishot", "Multishot", 1, EnchantmentRarity::Rare,
                    CROSSBOW, NONE, G::CROSSBOW_SHOT},
            {EnchantmentIds::PIERCING, "piercing", "Piercing", 4, EnchantmentRarity::Common,
                    CROSSBOW, NONE, G::CROSSBOW_SHOT},
            {EnchantmentIds::QUICK_CHARGE, "quick_charge", "Quick Charge", 3, EnchantmentRarity::Uncommon,
                    CROSSBOW, NONE, G::NONE},
            {EnchantmentIds::SOUL_SPEED, "soul_speed", "Soul Speed", 3, EnchantmentRarity::Mythic,
                    NONE, BOOTS, G::NONE},
            {EnchantmentIds::SWIFT_SNEAK, "swift_sneak", "Swift Sneak", 3, EnchantmentRarity::Mythic,
                    NONE, LEGGINGS, G::NONE},
            {EnchantmentIds::WIND_BURST, "wind_burst", "Wind Burst", 3, EnchantmentRarity::Mythic,
                    NONE, MACE, G::NONE},
            {EnchantmentIds::DENSITY, "density", "Density", 5, EnchantmentRarity::Common,
                    MACE, NONE, G::HEAVY_WEAPON_DAMAGE},
            {EnchantmentIds::BREACH, "breach", "Breach", 4, EnchantmentRarity::Rare,
                    MACE, NONE, G::HEAVY_WEAPON_DAMAGE},
            {EnchantmentIds::LUNGE, "lunge", "Lunge", 3, EnchantmentRarity::Uncommon,
                    SPEAR, NONE, G::NONE}
    };

    const size_t ENCHANTMENT_COUNT = sizeof(ENCHANTMENTS) / sizeof(ENCHANTMENTS[0]);

    std::string normalize(const std::string &input) {
        std::string result;
        result.reserve(input.size());

        size_t start = 0;
        if (input.rfind("minecraft:", 0) == 0) {
            start = 10;
        }

        for (size_t index = start; index < input.size(); ++index) {
            const char character = input[index];
            if (character == ' ' || character == '-') {
                result.push_back('_');
                continue;
            }
            result.push_back((char) std::tolower((unsigned char) character));
        }

        return result;
    }

    bool isNumeric(const std::string &value) {
        if (value.empty()) {
            return false;
        }

        for (char character: value) {
            if (character < '0' || character > '9') {
                return false;
            }
        }

        return true;
    }
}

const EnchantmentData *EnchantmentTable::getAll() {
    return ENCHANTMENTS;
}

size_t EnchantmentTable::getCount() {
    return ENCHANTMENT_COUNT;
}

const EnchantmentData &EnchantmentTable::at(size_t index) {
    return ENCHANTMENTS[index];
}

const EnchantmentData *EnchantmentTable::findById(int32_t id) {
    for (size_t index = 0; index < ENCHANTMENT_COUNT; ++index) {
        if (ENCHANTMENTS[index].mId == id) {
            return &ENCHANTMENTS[index];
        }
    }

    return nullptr;
}

const EnchantmentData *EnchantmentTable::findByName(const std::string &name) {
    const std::string normalized = normalize(name);

    for (size_t index = 0; index < ENCHANTMENT_COUNT; ++index) {
        if (normalized == ENCHANTMENTS[index].mName) {
            return &ENCHANTMENTS[index];
        }
    }

    return nullptr;
}

const EnchantmentData *EnchantmentTable::find(const std::string &input) {
    if (isNumeric(input)) {
        return findById((int32_t) strtol(input.c_str(), nullptr, 10));
    }

    return findByName(input);
}

int32_t EnchantmentTable::getRarityWeight(EnchantmentRarity rarity) {
    return (int32_t) rarity;
}

const char *EnchantmentTable::getRarityName(EnchantmentRarity rarity) {
    switch (rarity) {
        case EnchantmentRarity::Common:
            return "Common";
        case EnchantmentRarity::Uncommon:
            return "Uncommon";
        case EnchantmentRarity::Rare:
            return "Rare";
        case EnchantmentRarity::Mythic:
            return "Mythic";
        default:
            return "Unknown";
    }
}
