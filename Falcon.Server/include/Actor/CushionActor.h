#pragma once

#include "Actor/ServerActor.h"

class CushionActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:cushion";

    static constexpr int32_t SUPPORT_CHECK_PERIOD = 100;

    CushionActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override { return ActorSize{0.249f, 0.999f}; }

    Vector3f getSeatOffset() const override { return Vector3f(0.0f, 1.25f, 0.0f); }

    void setColor(uint8_t color) { mColor = color; }

    uint8_t getColor() const { return mColor; }

    void tick(ServerNetworkHandler &owner) override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) override;

    bool isExpired() const override { return mExpired; }

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    bool hasSupportingBlock(ServerNetworkHandler &owner) const;

    void breakCushion(ServerNetworkHandler &owner, bool dropItem);

    uint8_t mColor = 0;
    bool mExpired = false;
};
