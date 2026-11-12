#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class PlayerAuthInputPacket;
class ServerNetworkHandler;
class ServerPlayer;

// a gametest simulated player: the server writes its inputs, the movement pipeline plays them
class SimulatedPlayerController {
public:
    // LookDuration, in the game's order (sub_141039370)
    enum class LookDuration : uint8_t { Instant = 0, Continuous = 1, UntilMove = 2 };

    // the test the player was spawned by turns its relative moves and body angles
    void setTestRotation(int32_t rotation) {
        mTestRotation = rotation;
    }

    int32_t getTestRotation() const {
        return mTestRotation;
    }

    // move and moveRelative: a normalized direction, the speed argument is not read (sub_141037350)
    void moveInDirection(const Vector3f &direction, bool world);

    void moveToLocation(const Vector3f &target, bool faceTarget, float speed);

    void stopMoving();

    bool jump(ServerPlayer &player);

    void setSprinting(bool sprinting) {
        mSprinting = sprinting;
    }

    void lookAtLocation(ServerPlayer &player, const Vector3f &location, LookDuration duration);

    void lookAtEntity(ServerPlayer &player, int64_t uniqueId, const Vector3f &eyes, LookDuration duration);

    // sets the yaw and drops any look target (sub_141037CD0)
    void setBodyRotation(ServerPlayer &player, float yaw);

    // the next input, built from the current orders
    void buildInput(ServerNetworkHandler &owner, ServerPlayer &player, PlayerAuthInputPacket &packet);

    static float wrapDegrees(float angle);

private:
    enum class Move : uint8_t { None, Direction, ToLocation };
    enum class Look : uint8_t { None, Location, Entity };

    void _faceYaw(ServerPlayer &player, const Vector3f &target);

    void _lookAt(ServerPlayer &player, const Vector3f &target);

    Move mMove = Move::None;
    Vector3f mDirection;
    bool mWorldDirection = false;
    Vector3f mTarget;
    float mSpeed = 1.0f;

    Look mLook = Look::None;
    Vector3f mLookTarget;
    int64_t mLookEntity = 0;
    bool mLookTimed = false;
    bool mLookUntilMove = false;
    int32_t mLookTicks = 0;

    bool mJump = false;
    bool mSprinting = false;
    int32_t mTestRotation = 0;
};
