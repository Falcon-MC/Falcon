#pragma once

#include "World/Actor/ServerActor.h"

#include "Core/Math/Vector3i.h"

#include <optional>

class EndCrystalActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ender_crystal";

    static constexpr double EXPLOSION_SIZE = 6.0;

    static constexpr const char *SHOW_BOTTOM_TAG = "ShowBottom";

    // the BlockPos actor data the client draws the crystal's beam to
    static constexpr int32_t BLOCK_TARGET_DATA_ID = 47;

    EndCrystalActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override {
        return ActorSize{0.98f, 0.98f};
    }

    void tick(ServerNetworkHandler &owner) override;

    bool isExpired() const override {
        return mExpired;
    }

    bool isInvulnerable() const override {
        return mInvulnerable;
    }

    void setInvulnerable(bool invulnerable) {
        mInvulnerable = invulnerable;
    }

    const std::optional<Vector3i> &getBeamTarget() const {
        return mBeamTarget;
    }

    void setBeamTarget(ServerNetworkHandler &owner, const std::optional<Vector3i> &target);

    void setShowBottom(bool showBottom);

    bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    void _appendBeamTarget(EntityDataMap &metadata) const;

    bool mExpired = false;
    bool mInvulnerable = false;
    std::optional<Vector3i> mBeamTarget;
};
