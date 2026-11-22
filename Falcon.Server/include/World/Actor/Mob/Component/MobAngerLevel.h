#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <vector>

class Actor;
class MobActor;
class ServerNetworkHandler;
class Tag;

// minecraft:anger_level: how angry the mob is at each actor that disturbed it, highest first
class MobAngerLevel {
public:
    struct Nuisance {
        int64_t mActorId = -1;
        int32_t mAnger = 0;
        // set for anything but a player: players come first among equally angry entries
        bool mPriority = false;
    };

    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void increase(ServerNetworkHandler &owner, MobActor &mob, Actor &target, int32_t amount, bool playSound);

    bool isValidTarget(ServerNetworkHandler &owner, const MobActor &mob, const Actor &target) const;

    // the first loaded and valid entry, null when there is none
    Actor *getActiveEntity(ServerNetworkHandler &owner, const MobActor &mob) const;

    // the anger of the first entry, valid or not, 0 when there is none
    int32_t getHighestAnger() const {
        return mNuisances.empty() ? 0 : mNuisances.front().mAnger;
    }

    int32_t getAngerAt(int64_t actorId) const;

    // what query.anger_level reads: raised by every increase, set again at each decrement
    int32_t getLevel() const {
        return mLevel;
    }

    const std::vector<Nuisance> &getNuisances() const {
        return mNuisances;
    }

    void save(Tag &data) const;

    void load(const Tag &data);

    static int32_t defaultAnnoyingness(const MobActor &mob);

    static int32_t projectileAnnoyingness(const MobActor &mob);

    static int32_t angryThreshold(const MobActor &mob);

    static int32_t angryBoost(const MobActor &mob);

private:
    void _decrement(ServerNetworkHandler &owner, MobActor &mob);

    void _insert(const Nuisance &nuisance, int32_t threshold);

    void _playIncreaseSound(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component) const;

    std::vector<Nuisance> mNuisances;
    int32_t mLevel = 0;
    int64_t mNextDecrementTick = -1;
};
