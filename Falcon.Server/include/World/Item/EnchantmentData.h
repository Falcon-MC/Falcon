#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace EnchantmentIds {
    const int32_t PROTECTION = 0;
    const int32_t FIRE_PROTECTION = 1;
    const int32_t FEATHER_FALLING = 2;
    const int32_t BLAST_PROTECTION = 3;
    const int32_t PROJECTILE_PROTECTION = 4;
    const int32_t THORNS = 5;
    const int32_t RESPIRATION = 6;
    const int32_t DEPTH_STRIDER = 7;
    const int32_t AQUA_AFFINITY = 8;
    const int32_t SHARPNESS = 9;
    const int32_t SMITE = 10;
    const int32_t BANE_OF_ARTHROPODS = 11;
    const int32_t KNOCKBACK = 12;
    const int32_t FIRE_ASPECT = 13;
    const int32_t LOOTING = 14;
    const int32_t EFFICIENCY = 15;
    const int32_t SILK_TOUCH = 16;
    const int32_t UNBREAKING = 17;
    const int32_t FORTUNE = 18;
    const int32_t POWER = 19;
    const int32_t PUNCH = 20;
    const int32_t FLAME = 21;
    const int32_t INFINITY_ENCHANTMENT = 22;
    const int32_t LUCK_OF_THE_SEA = 23;
    const int32_t LURE = 24;
    const int32_t FROST_WALKER = 25;
    const int32_t MENDING = 26;
    const int32_t BINDING = 27;
    const int32_t VANISHING = 28;
    const int32_t IMPALING = 29;
    const int32_t RIPTIDE = 30;
    const int32_t LOYALTY = 31;
    const int32_t CHANNELING = 32;
    const int32_t MULTISHOT = 33;
    const int32_t PIERCING = 34;
    const int32_t QUICK_CHARGE = 35;
    const int32_t SOUL_SPEED = 36;
    const int32_t SWIFT_SNEAK = 37;
    const int32_t WIND_BURST = 38;
    const int32_t DENSITY = 39;
    const int32_t BREACH = 40;
    const int32_t LUNGE = 41;
}

namespace EnchantmentTargets {
    const uint32_t NONE = 0u;
    const uint32_t HELMET = 1u << 0;
    const uint32_t CHESTPLATE = 1u << 1;
    const uint32_t LEGGINGS = 1u << 2;
    const uint32_t BOOTS = 1u << 3;
    const uint32_t SHIELD = 1u << 4;
    const uint32_t SWORD = 1u << 5;
    const uint32_t TRIDENT = 1u << 6;
    const uint32_t SPEAR = 1u << 7;
    const uint32_t MACE = 1u << 8;
    const uint32_t BOW = 1u << 9;
    const uint32_t CROSSBOW = 1u << 10;
    const uint32_t SHEARS = 1u << 11;
    const uint32_t FLINT_AND_STEEL = 1u << 12;
    const uint32_t AXE = 1u << 13;
    const uint32_t PICKAXE = 1u << 14;
    const uint32_t SHOVEL = 1u << 15;
    const uint32_t HOE = 1u << 16;
    const uint32_t FISHING_ROD = 1u << 17;
    const uint32_t CARROT_ON_STICK = 1u << 18;
    const uint32_t COMPASS = 1u << 19;
    const uint32_t MASK = 1u << 20;
    const uint32_t ELYTRA = 1u << 21;
    const uint32_t BRUSH = 1u << 22;
    const uint32_t BOOK = 1u << 23;

    const uint32_t ARMOR = HELMET | CHESTPLATE | LEGGINGS | BOOTS;
    const uint32_t BLOCK_TOOLS = AXE | PICKAXE | SHOVEL | HOE;
    const uint32_t WEAPONS = SWORD | TRIDENT | SPEAR | MACE | BOW | CROSSBOW | BLOCK_TOOLS;
    const uint32_t DURABLE_MISC = SHEARS | FLINT_AND_STEEL | SHIELD | CARROT_ON_STICK | ELYTRA | BRUSH;
    const uint32_t ALL = ARMOR | SHIELD | SWORD | TRIDENT | SPEAR | MACE | BOW | CROSSBOW | SHEARS |
                         FLINT_AND_STEEL | BLOCK_TOOLS | FISHING_ROD | CARROT_ON_STICK | COMPASS | MASK |
                         ELYTRA | BRUSH | BOOK;
}

namespace EnchantmentGroups {
    const uint32_t NONE = 0u;
    const uint32_t PROTECTION = 1u << 0;
    const uint32_t WEAPON_DAMAGE = 1u << 1;
    const uint32_t BLOCK_DROPS = 1u << 2;
    const uint32_t BOW_INFINITE = 1u << 3;
    const uint32_t TRIDENT_PROPULSION = 1u << 4;
    const uint32_t CROSSBOW_SHOT = 1u << 5;
    const uint32_t BOOTS_MOVEMENT = 1u << 6;
    const uint32_t HEAVY_WEAPON_DAMAGE = 1u << 7;
}

enum class EnchantmentRarity : int32_t {
    Common = 10,
    Uncommon = 5,
    Rare = 2,
    Mythic = 1
};

struct EnchantmentData {
    int32_t mId;
    const char *mName;
    const char *mDisplayName;
    int32_t mMaxLevel;
    EnchantmentRarity mRarity;
    uint32_t mPrimaryTargets;
    uint32_t mSecondaryTargets;
    uint32_t mIncompatibleGroups;
};

class EnchantmentTable {
public:
    static const EnchantmentData *getAll();

    static size_t getCount();

    static const EnchantmentData &at(size_t index);

    static const EnchantmentData *findById(int32_t id);

    static const EnchantmentData *findByName(const std::string &name);

    static const EnchantmentData *find(const std::string &input);

    static int32_t getRarityWeight(EnchantmentRarity rarity);

    static const char *getRarityName(EnchantmentRarity rarity);
};
