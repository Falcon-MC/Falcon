#pragma once

#include "World/Actor/ServerActor.h"

class Level;
class ServerPlayer;

class FishingHookActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:fishing_hook";

    FishingHookActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override { return ActorSize{0.15f, 0.15f}; }

    void tick(ServerNetworkHandler &owner) override;

    bool isExpired() const override { return mExpired; }

    bool shouldSave() const override { return false; }

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    /** Gives the motion of a freshly cast hook, aimed along the angler's view. */
    void cast(ServerNetworkHandler &owner, ServerPlayer &angler);

    /** Reels the hook in, removes it and returns the durability the rod loses. */
    int32_t retrieve(ServerNetworkHandler &owner, ServerPlayer &angler);

    bool isOwnedBy(const ServerPlayer &player) const;

private:
    ServerPlayer *_findAngler(ServerNetworkHandler &owner) const;

    bool _anglerKeepsHook(const ServerPlayer &angler) const;

    void _tickHooked(ServerNetworkHandler &owner);

    void _move(ServerNetworkHandler &owner, Level &level, float waterFraction);

    void _tryHookActor(ServerNetworkHandler &owner, const Vector3f &from, const Vector3f &to);

    void _tickCatching(ServerNetworkHandler &owner, Level &level, const ServerPlayer &angler);

    int32_t _catchRate(Level &level) const;

    void _startLuring(ServerNetworkHandler &owner, const ServerPlayer &angler);

    void _catch(ServerNetworkHandler &owner, ServerPlayer &angler);

    void _pullTowards(ServerNetworkHandler &owner, Actor &target, const ServerPlayer &angler) const;

    float _waterFraction(Level &level) const;

    void _setHooked(ServerNetworkHandler &owner, int64_t uniqueId);

    void _sendFishData(ServerNetworkHandler &owner, float x, float z, const float *angle);

    bool mExpired = false;
    bool mInGround = false;
    int64_t mHookedUniqueId = 0;
    int64_t mAnglerUniqueId = 0;
    int32_t mTimeUntilLured = 0;
    int32_t mTimeUntilHooked = 0;
    int32_t mNibble = 0;
    float mFishAngle = 0.0f;
    float mBobTime = 0.0f;
};
