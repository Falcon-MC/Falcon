#pragma once

#include "Actor/SizedActor.h"

class ExperienceOrbActor final : public SizedActor {
public:
    ExperienceOrbActor(uint64_t runtimeId, const std::string &identifier)
            : SizedActor(runtimeId, identifier, ActorSize{0.25f, 0.25f}, false) {
    }

    bool isPushable() const override {
        return false;
    }

    bool isExpired() const override {
        return mExpired;
    }

    void tick(ServerNetworkHandler &owner) override;

private:
    bool _tryPickup(ServerNetworkHandler &owner, ServerPlayer &player);

    void _attract(const ServerPlayer &player, Vector3f &motion) const;

    bool mExpired = false;
};
