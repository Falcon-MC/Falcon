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
class ServerPlayer;
class UpdateTradePacket;

// minecraft:economy_trade_table: the offers, the trader's tier and experience, saved with the mob
class MobEconomy {
public:
    static constexpr const char *COMPONENT = "minecraft:economy_trade_table";

    static constexpr int32_t TRADE_TARGET_DATA_ID = 68;
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

    // one player at a time; prices follow that player's hero effect and the villager's cure discounts
    bool canTradeWith(ServerNetworkHandler &owner, const MobActor &mob, const ServerPlayer &player) const;

    // false without offers; fills the packet but its window id
    bool startTrading(ServerNetworkHandler &owner, MobActor &mob, ServerPlayer &player, UpdateTradePacket &packet);

    void stopTrading(ServerNetworkHandler &owner, MobActor &mob);

    // the trading player while that player is still here
    ServerPlayer *findTradingPlayer(ServerNetworkHandler &owner) const;

    int64_t getTradingPlayer() const {
        return mTradingPlayer;
    }

    // uses, the player's experience, the trader's experience and tier after a granted trade
    void onTrade(ServerNetworkHandler &owner, MobActor &mob, int32_t offerIndex, int32_t crafts);

    // a trader that reached a new tier rewards the player and regenerates once the trade screen is closed
    void tick(ServerNetworkHandler &owner, MobActor &mob);

    // keep_level of a transformation: the tier, the experience and the cure discounts
    void inheritLevel(const MobEconomy &from);

    // offers carried by a transformation; a mob without the component keeps them aside until it trades again
    void inheritOffers(ServerNetworkHandler &owner, MobActor &mob, MobEconomy &from, MobActor &fromMob);

    // a cured zombie villager trades cheaper; villagers around it get the low tier discount
    void applyCure(const MobActor &mob);

    void applyNearbyCure(const MobActor &mob);

    bool hasPersistingOffers() const {
        return mHasPersistingOffers;
    }

    void appendMetadata(EntityDataMap &metadata) const;

    void saveNbt(Tag &data) const;

    // any mob: a zombie villager keeps the offers of the villager it was
    void savePersistingNbt(Tag &data, bool trader) const;

    void loadNbt(const Tag &data);

    static std::mt19937 &random();

private:
    void _generate(ServerNetworkHandler &owner, MobActor &mob, const std::string &path);

    void _syncMetadata(ServerNetworkHandler &owner, MobActor &mob) const;

    bool _isTrading(ServerNetworkHandler &owner, const MobActor &mob) const;

    void _setSavedOffers(const Tag &offers);

    std::unique_ptr<MerchantOffers> mOffers;
    Tag mPersistingOffers;
    bool mHasPersistingOffers = false;
    int32_t mPersistingRiches = 0;
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
    int64_t mTradingPlayer = NO_PLAYER;
    int32_t mTierUpCountdown = 0;
    int64_t mTierUpPlayer = NO_PLAYER;

    static constexpr int64_t NO_PLAYER = -1;
};
