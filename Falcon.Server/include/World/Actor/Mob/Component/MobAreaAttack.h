#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <string>
#include <unordered_map>

class Actor;
class MobActor;
class ServerNetworkHandler;

// minecraft:area_attack: hurts what the entity filter accepts inside the box grown by damage_range
class MobAreaAttack {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    struct Settings {
        float mRange = 0.2f;
        float mDamage = 2.0f;
        int64_t mCooldownTicks = 0;
        std::string mCause;
        std::string mDeathMessageKey;
        bool mSelfAsSource = false;
        const json::Value *mFilter = nullptr;
    };

    void _read(const json::Value &component);

    void _attack(ServerNetworkHandler &owner, MobActor &mob, Actor &target);

    const json::Value *mComponent = nullptr;
    Settings mSettings;
    std::unordered_map<int64_t, int64_t> mCooldowns;
};
