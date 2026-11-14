#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>

// an item worn in the body slot: horse, nautilus and wolf armor
struct BodyArmorData {
    const char *mIdentifier;
    int32_t mPoints;
    int32_t mToughness;
    float mKnockbackResistance;
    // the wolf armor is an armor piece and wears down when its wearer is hit; horse and nautilus armor never do
    bool mWearsOnHit;
    // null for the pieces that never break
    const char *mBreakSound;
};

class BodyArmor {
public:
    static const BodyArmorData *find(const std::string &identifier);

    static const BodyArmorData *find(const ItemStack &item);
};
