#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <vector>

struct MerchantOffer {
    ItemStack mBuyA = ItemStack::air();
    ItemStack mBuyB = ItemStack::air();
    ItemStack mSell = ItemStack::air();
    int32_t mTier = 0;
    int32_t mUses = 0;
    int32_t mMaxUses = 7;
    int32_t mTraderExp = 0;
    bool mRewardExp = true;
    int32_t mDemand = 0;
    // the first wanted count before any demand or discount; buyA holds the price paid now
    int32_t mBuyCountA = 0;
    int32_t mBuyCountB = 0;
    float mPriceMultiplierA = 0.0f;
    float mPriceMultiplierB = 0.0f;
    int32_t mNetId = 0;

    // a negative maximum never runs out
    bool isOutOfStock() const {
        return mMaxUses >= 0 && mUses >= mMaxUses;
    }

    // cure discounts are the villager's, already clamped; heroAmplifier counts only with hero
    void updatePrice(int32_t lowCureDiscount, int32_t highCureDiscount, bool hero, int32_t heroAmplifier);

    // demand grows with the uses past half the maximum, then every use comes back
    void resupply();
};

class MerchantOffers {
public:
    std::vector<MerchantOffer> mOffers;
    // the trader experience each tier starts at
    std::vector<int32_t> mTierExpRequirements;
    // the highest tier the trader sells now: the trade packet lists every tier, locked ones included. not saved
    int32_t mUnlockedTier = INT32_MAX;

    // missing net ids are drawn from the same counter as generated offers
    static MerchantOffers load(const Tag &data, const PacketCodecContext &context);

    // the save keeps no net id; the trade packet carries them
    Tag save(bool withNetIds) const;

    static int32_t nextNetId();

    MerchantOffer *findByNetId(int32_t netId);

    int32_t indexOfNetId(int32_t netId) const;

    // the last tier whose requirement the experience reaches
    int32_t tierForExperience(int32_t experience) const;
};
