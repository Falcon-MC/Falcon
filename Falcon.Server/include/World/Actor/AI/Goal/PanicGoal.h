#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// minecraft:behavior.panic (PanicGoal, vtable 0x14F3437B0)
class PanicGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 1.0f;
        bool mIgnoreMobDamage = false;
        bool mForce = false;
        bool mPreferWater = false;
        // the game's default when the definition lists none (xmmword_14F343708)
        std::vector<std::string> mDamageSources = {
                "campfire",         "fire",      "fire_tick",     "freezing",    "lava",
                "lightning",        "magma",     "soul_campfire", "temperature", "entity_attack",
                "entity_explosion", "fireworks", "magic",         "projectile",  "ram_attack",
                "sonic_boom",       "wither",    "mace_smash"};
    };

    explicit PanicGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _shouldPanic(ServerNetworkHandler &owner, MobActor &mob) const;

    std::optional<Vector3f> _findTarget(ServerNetworkHandler &owner, MobActor &mob) const;

    Settings mSettings;
    bool mAnySource = false;
    Vector3f mTarget;
    int64_t mNextRepathTick = 0;
};
