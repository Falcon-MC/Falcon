#pragma once

#include <cstdint>

class ServerActor;
class ServerNetworkHandler;

// minecraft:conditional_bandwidth_optimization: far actors skip move packets to a viewer for a few ticks
class BandwidthOptimization {
public:
    struct Values {
        float mMaxOptimizedDistance = 80.0f;
        int64_t mMaxDroppedTicks = 10;
        bool mUseMotionPredictionHints = false;
    };

    // false: the actor has no such component and sends every move
    static bool valuesOf(ServerNetworkHandler &owner, const ServerActor &actor, Values &values);

    // ticks a viewer this far away may go without a move packet
    static int64_t allowedGap(const Values &values, float distance);
};
