#pragma once

class Actor;
class MobActor;
class ServerNetworkHandler;

// a minecraft:apply_knockback_rules preset, with the game's own knockback for the fields it leaves out
struct KnockbackPreset {
    float mHorizontalPower = 0.4f;
    float mVerticalPower = 0.4f;
    float mVerticalVelocityCap = 0.4f;
    float mSlowdownScale = 0.5f;
};

namespace KnockbackRules {
    // the attacker's first preset whose filter takes the target; false: no preset applies, the usual knockback does
    bool select(ServerNetworkHandler &owner, const MobActor &attacker, const Actor &target, KnockbackPreset &preset);

    // the top and the bottom of the bounding box both in water
    bool isImmersedInWater(ServerNetworkHandler &owner, const Actor &actor);
}
