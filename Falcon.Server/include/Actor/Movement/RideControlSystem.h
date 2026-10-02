#pragma once

#include <cstdint>

class MobActor;
class PlayerAuthInputPacket;
class ServerNetworkHandler;
class ServerPlayer;

enum class RideControlType {
    None,
    Ground,
    Air,
    Water,
    Item
};

struct RiderInput {
    float mStrafe = 0.0f;
    float mForward = 0.0f;
    float mRawStrafe = 0.0f;
    float mRawForward = 0.0f;
    float mYaw = 0.0f;
    float mPitch = 0.0f;
    bool mJumpHeld = false;
    bool mSprinting = false;
    bool mReceived = false;
};

struct RideControlState {
    RiderInput mInput;
    int32_t mJumpChargeTicks = -1;
    int32_t mDashChargeTicks = -1;
    bool mWaterDashStartedInWater = false;
    int64_t mDashCooldownEndTick = -1;
    int64_t mJumpTick = -1;
    int32_t mRearingTicks = 0;
    int32_t mBoostTicks = 0;
    float mBoostMultiplier = 1.0f;
    bool mAirRotationInitialized = false;
    float mAirTargetYaw = 0.0f;
    int32_t mAirRotationDelayTicks = 0;
    bool mWasOnSlipperyGround = false;
    int32_t mSlipperyGraceTicks = 0;
    float mSlipperyEntrySpeed = 0.0f;
};

class RideControlSystem {
public:
    static RideControlType controlTypeOf(const MobActor &mob);

    static bool isControlledBy(const MobActor &mob, const ServerPlayer &player);

    static ServerPlayer *controllerOf(ServerNetworkHandler &owner, const MobActor &mob);

    static void receiveInput(ServerNetworkHandler &owner, ServerPlayer &player, const PlayerAuthInputPacket &packet);

    static bool tick(ServerNetworkHandler &owner, MobActor &mob);

    static bool tryBoost(ServerNetworkHandler &owner, ServerPlayer &player);

    static void syncFlags(ServerNetworkHandler &owner, MobActor &mob);

    static void reset(MobActor &mob);

private:
    static bool _holdsControlItem(const MobActor &mob, const ServerPlayer &player);

    static bool _handleJumpOrDash(ServerNetworkHandler &owner, MobActor &mob, RideControlType type);

    static bool _tryDash(ServerNetworkHandler &owner, MobActor &mob, RideControlType type, float charge);

    static void _tickGround(ServerNetworkHandler &owner, MobActor &mob);

    static void _tickAir(MobActor &mob);

    static void _tickWater(ServerNetworkHandler &owner, MobActor &mob);

    static void _tickItem(MobActor &mob);

    static void _tickCooldowns(ServerNetworkHandler &owner, MobActor &mob);

    static void _applyAirRotation(MobActor &mob, float targetYaw);

    static void _applySlipperyImpulse(MobActor &mob, float wishX, float wishZ, float strength, float cap,
                                      float controlScale, float slipperiness);

    static void _applySlipperyLookInput(MobActor &mob, float wishX, float wishZ, float strength, float cap,
                                        float slipperiness);
};
