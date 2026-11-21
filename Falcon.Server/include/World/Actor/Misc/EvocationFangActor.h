#pragma once

#include "World/Actor/SizedActor.h"

// the evoker's fangs: standing still, they bite once early in their short life
class EvocationFangActor final : public SizedActor {
public:
    // the life a fang gets when nothing summoned it with one
    static constexpr int32_t LIFESPAN_TICKS = 22;
    // the bite comes when this much life is left
    static constexpr int32_t BITE_TICKS = 20;
    static constexpr float BITE_DAMAGE = 6.0f;

    EvocationFangActor(uint64_t runtimeId, const std::string &identifier)
            : SizedActor(runtimeId, identifier, ActorSize{1.0f, 0.8f}, false) {}

    bool hasGravity() const override {
        return false;
    }

    bool isPushable() const override {
        return false;
    }

    void tick(ServerNetworkHandler &owner) override;

private:
    void _bite(ServerNetworkHandler &owner);
};
