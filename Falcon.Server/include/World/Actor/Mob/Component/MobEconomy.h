#pragma once

#include "Core/NBT/Tag.h"
#include "World/Actor/Trade/MerchantOffers.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>

class EntityDataMap;
class MobActor;
class ServerNetworkHandler;

// minecraft:economy_trade_table: the offers, the trader's tier and experience, saved with the mob
class MobEconomy {
public:
    static constexpr const char *COMPONENT = "minecraft:economy_trade_table";

    static constexpr int32_t TRADE_TIER_DATA_ID = 101;
    static constexpr int32_t MAX_TRADE_TIER_DATA_ID = 102;
    static constexpr int32_t TRADE_EXPERIENCE_DATA_ID = 103;
    static constexpr int32_t LOW_TIER_CURED_DISCOUNT_DATA_ID = 114;
    static constexpr int32_t HIGH_TIER_CURED_DISCOUNT_DATA_ID = 115;
    static constexpr int32_t NEARBY_CURED_DISCOUNT_DATA_ID = 116;
    static constexpr int32_t NEARBY_CURED_DISCOUNT_TIME_STAMP_DATA_ID = 117;

    static bool isTrader(const MobActor &mob);

    // rolls the table when there are no offers yet or the table changed, like a new profession; nullptr without
    // a table
    MerchantOffers *getOffers(ServerNetworkHandler &owner, MobActor &mob);

    const MerchantOffers *peekOffers() const {
        return mOffers.get();
    }

    // every use comes back; a save not read yet is resupplied once it is
    void resupply();

    // the tier follows the experience; experience under the tier's own requirement is raised to it
    void syncTier();

    int32_t getTier() const {
        return mTier;
    }

    int32_t getMaxTier() const {
        return mMaxTier;
    }

    int32_t getExperience() const {
        return mExperience;
    }

    int32_t getRiches() const {
        return mRiches;
    }

    void appendMetadata(EntityDataMap &metadata) const;

    void saveNbt(Tag &data) const;

    void loadNbt(const Tag &data);

    static std::mt19937 &random();

private:
    void _generate(ServerNetworkHandler &owner, MobActor &mob, const std::string &path);

    std::unique_ptr<MerchantOffers> mOffers;
    Tag mSavedOffers;
    bool mHasSavedOffers = false;
    bool mResupplyPending = false;
    bool mConvertedFromVillagerV1 = false;
    std::string mTablePath;
    int32_t mRiches = 0;
    int32_t mTier = 0;
    int32_t mMaxTier = 0;
    int32_t mExperience = 0;
    int32_t mLowTierCuredDiscount = 0;
    int32_t mHighTierCuredDiscount = 0;
    int32_t mNearbyCuredDiscount = 0;
    int32_t mNearbyCuredDiscountTimeStamp = 0;
};
