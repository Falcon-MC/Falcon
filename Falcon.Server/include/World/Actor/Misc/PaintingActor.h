#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "World/Actor/ServerActor.h"

class Level;
class ServerPlayer;

class PaintingActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:painting";

    PaintingActor(uint64_t runtimeId, const std::string &identifier);

    static bool place(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &support, int32_t face);

    ActorSize getSize() const override;

    AxisAlignedBB getPaintingBounds() const;

    void sendPaintingSpawn(ServerNetworkHandler &owner, const ServerPlayer &player) const;

    void tick(ServerNetworkHandler &owner) override;
    void tickMovement(ServerNetworkHandler &) override;
    bool isExpired() const override;
    bool shouldSave() const override;
    bool isPushable() const override;
    bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) override;
    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    Tag saveNbt() const override;
    void loadNbt(const Tag &data) override;

private:
    size_t mMotive = 0;
    int32_t mDirection = 0;
    int32_t mSupportCheckTicks = 0;
    bool mExpired = false;
};
