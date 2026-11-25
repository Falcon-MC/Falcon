#pragma once

#include <cstdint>
#include <optional>

class MobActor;
class ServerNetworkHandler;

// minecraft:mob_effect: gives an effect to what stands near the mob, every cooldown
class MobEffectAura {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    std::optional<int64_t> mNextTick;
};
