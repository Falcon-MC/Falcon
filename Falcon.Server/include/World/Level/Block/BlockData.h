#pragma once

#include <cstddef>
#include <cstdint>

enum class BlockToolType : uint8_t {
    None = 0,
    Sword = 1,
    Shovel = 2,
    Pickaxe = 3,
    Axe = 4,
    Shears = 5,
    Hoe = 6,
    Spear = 7
};

enum class BlockToolTier : uint8_t {
    None = 0,
    Wooden = 1,
    Gold = 2,
    Stone = 3,
    Copper = 4,
    Iron = 5,
    Diamond = 6,
    Netherite = 7
};

enum class BlockDropKind : uint8_t { Nothing = 0, Self = 1, Other = 2 };

// set for about 190 blocks and air, every other block
// obstructs, like the explicit obstruct_rain of ice, packed ice and grass path
enum class BlockPrecipitationInteraction : uint8_t {
    ObstructRain,
    None,
    // a snow layer may be placed in the block's position
    Snowlogging
};

struct BlockData {
    int32_t mTypeId;
    const char *mIdentifier;
    const char *mName;
    float mHardness;
    float mResistance;
    BlockToolType mToolType;
    BlockToolTier mToolTier;
    bool mHandHarvest;
    bool mSilkTouch;
    uint8_t mLightEmission;
    bool mTransparent;
    bool mSolid;
    uint8_t mBurnChance;
    uint8_t mBurnAbility;
    BlockDropKind mDropKind;
    const char *mDropIdentifier;
    uint8_t mDropMin;
    uint8_t mDropMax;
    BlockPrecipitationInteraction mPrecipitation = BlockPrecipitationInteraction::ObstructRain;
    const char *mBehaviorIdentifier = nullptr;
    int32_t mMaxStackSize = 64;
};

class BlockDataTable {
public:
    static const BlockData *getEntries();

    static size_t getCount();

    static const BlockData *find(const char *identifier);

    static const BlockData *findByTypeId(int32_t typeId);
};
