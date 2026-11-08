#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;

/**
 * The block actor of an end gateway (EndGatewayBlockActor: load sub_14714E3D0, save sub_14714E630, tick
 * sub_14714E830). Its exit is found by the dragon fight once both ends are loaded; the gateway then sends what
 * enters it there, one entity per cooldown.
 */
class EndGatewayBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "EndGateway";

    // the client draws the purple beam while the age is below this
    static constexpr int32_t SPAWN_BEAM_TICKS = 200;

    static constexpr int32_t COOLDOWN_TICKS = 20;

    // the block event that starts the cooldown beam on the client (sub_14714F7F0)
    static constexpr int32_t COOLDOWN_EVENT = 1;

    // older worlds saved exits near the origin: these are found again once (sub_14714E3D0)
    static constexpr int64_t BAD_EXIT_DISTANCE = 99;

    static constexpr int64_t BAD_GATEWAY_DISTANCE = 299;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    /** A gateway the game would have loaded without an exit: it asks for one on its next ticking tick. */
    void requestExit() {
        mSearchExit = true;
    }

    const Vector3i &getExit() const noexcept {
        return mExit;
    }

    bool hasExit() const;

    /** The exit found by the dragon fight, which needs no further check. */
    void setExit(const Vector3i &exit);

    int32_t getAge() const noexcept {
        return mAge;
    }

    int32_t getCooldown() const noexcept {
        return mCooldown;
    }

    bool isExitSearchPending() const noexcept {
        return mSearchExit;
    }

    bool isExitCheckPending() const noexcept {
        return mCheckExit;
    }

    bool isBadPosition() const noexcept {
        return mBadPosition;
    }

    /** Ages the gateway and runs down its cooldown. False while it cools down: nothing goes through this tick. */
    bool startTick();

    /** Whether the client must get the gateway again after this tick: the spawn beam or the cooldown changed. */
    bool needsClientUpdate() const;

    /** Starts the cooldown after a teleport (sub_14714F7F0 on the client). */
    void triggerCooldown() {
        mCooldown = COOLDOWN_TICKS;
    }

    /** Where an entity sent through lands: one block above the exit, centred (sub_14714E830). */
    static Vector3f getTeleportTarget(const Vector3i &exit);

    static bool isBadPosition(const Vector3i &exit, const Vector3i &gateway);

private:
    void _writeFields(Tag &data) const;

    void _requestExitWork(ServerNetworkHandler &owner, Level &level);

    void _teleportEntities(ServerNetworkHandler &owner, Level &level);

    void _teleport(ServerNetworkHandler &owner, Level &level, Actor &actor);

    static bool _canTeleport(const Actor &actor);

    int32_t mAge = 0;
    int32_t mCooldown = 0;
    Vector3i mExit;
    bool mSearchExit = false;
    bool mCheckExit = false;
    bool mBadPosition = false;
    int32_t mPreviousAge = 0;
    bool mWasCooling = false;
};
