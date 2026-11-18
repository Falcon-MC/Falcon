#pragma once

#include "World/Actor/Mob/Passive/PassiveActor.h"

#include <cstdint>

class VillagerV2Actor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:villager_v2";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override {
        return ActorSize{0.6f, 1.9f};
    }

    float getDefaultMaxHealth() const override {
        return 20.0f;
    }

    int getExperienceDrop() const override {
        return 0;
    }

    bool isNearbyCureTarget() const override {
        return true;
    }

    // a sleeping villager is only woken up
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    // the InLove flag with the partner it names, no timer: the make_love goal ends it
    void setLovePartner(ServerNetworkHandler &owner, const Actor *partner);

    int64_t getLovePartnerId() const {
        return mLovePartnerId;
    }

    bool hasLovePartner() const {
        return mLovePartnerId != NO_PARTNER;
    }

    // the one of the pair that started: only it spawns the baby
    bool isLoveInitiator() const {
        return mLoveInitiator;
    }

    void setLoveInitiator(bool initiator) {
        mLoveInitiator = initiator;
    }

    // set only by a saved villager; breeding clears it
    bool isWilling() const {
        return mWilling;
    }

    void setWilling(bool willing) {
        mWilling = willing;
    }

    // the first inventory slot holding 3 bread, or 12 carrots, potatoes or beetroots; -1 for none
    int findBreedingFoodSlot() const;

    // the food findBreedingFoodSlot found leaves the inventory
    void consumeBreedingFood();

private:
    int64_t mLovePartnerId = NO_PARTNER;
    bool mLoveInitiator = false;
    bool mWilling = false;

    static constexpr int64_t NO_PARTNER = -1;
};
