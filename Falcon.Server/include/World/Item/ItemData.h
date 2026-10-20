#pragma once

#include "World/Item/Components/ItemComponents.h"
#include "World/Item/ItemTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>

struct ItemData {
    int32_t mTypeId;
    const char *mIdentifier;
    const char *mName;
    int32_t mMaxStackSize;
    int32_t mMaxDurability;
    ToolType mToolType;
    int32_t mToolTier;
    int32_t mHarvestLevel;
    int32_t mAttackDamage;
    ArmorSlot mArmorSlot;
    int32_t mArmorTier;
    int32_t mArmorPoints;
    int32_t mArmorToughness;
    int32_t mNutrition;
    float mSaturation;
};

class ItemDataTable {
public:
    static const ItemData *getAll();

    static size_t getCount();

    static const ItemData &at(size_t index);

    static const ItemData *find(const std::string &identifier);

    static const ItemComponents &getComponents(const std::string &identifier);
};
