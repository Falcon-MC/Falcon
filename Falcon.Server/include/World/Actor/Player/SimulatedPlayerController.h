#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"

#include <chrono>
#include <cstdint>
#include <vector>

class Actor;
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

    // sub_141036BB0: the jump goes out now and the delay starts, a second jump the same tick does nothing
    bool jump(ServerNetworkHandler &owner, ServerPlayer &player);

    void setSprinting(bool sprinting) {
        mSprinting = sprinting;
    }

    // Actor::startSwimming and stopSwimming (vtable +0x310, +0x318), played as the client's swim inputs
    void setSwimming(bool swimming) {
        mSwimRequest = swimming ? Request::Start : Request::Stop;
    }

    // false when already gliding, without an elytra, in water or on the ground
    bool glide(ServerNetworkHandler &owner, ServerPlayer &player);

    // sub_1402C7EF0
    void stopGliding() {
        mGlideRequest = Request::Stop;
    }

    void lookAtLocation(ServerPlayer &player, const Vector3f &location, LookDuration duration);

    void lookAtEntity(ServerPlayer &player, int64_t uniqueId, const Vector3f &eyes, LookDuration duration);

    // sets the yaw and drops any look target (sub_141037CD0)
    void setBodyRotation(ServerPlayer &player, float yaw);

    // attacks and item uses wait this many ticks after the previous one (sub_141038210, sub_141038670)
    static constexpr int64_t ACTION_COOLDOWN_TICKS = 10;

    // sub_141038210: the swing, then the target if any; no reach nor facing check
    bool attack(ServerNetworkHandler &owner, ServerPlayer &player, Actor *target);

    // Player::interact with no click position (sub_1402D2520)
    bool interactWithEntity(ServerNetworkHandler &owner, ServerPlayer &player, Actor &target);

    // a click on a block face with the held item: the client's ItemUse transaction
    bool interactWithBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block, int32_t face,
                           const Vector3f &clickPosition);

    bool useBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block, int32_t face);

    // sub_14DCFD060, sub_141038670: false when dead, without an item or still cooling down
    bool useItem(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item);

    bool useItemInSlot(ServerNetworkHandler &owner, ServerPlayer &player, int32_t slot);

    // sub_141038440: the item held for the click, the selected slot restored after
    bool useItemOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item, const Vector3i &block,
                        int32_t face, const Vector3f &clickPosition);

    bool useItemInSlotOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, int32_t slot, const Vector3i &block,
                              int32_t face, const Vector3f &clickPosition);

    // the item in use as it was before the release, air when none (sub_14DCFD8B0)
    ItemStack stopUsingItem(ServerNetworkHandler &owner, ServerPlayer &player);

    // sub_141039180: true while it hits the block; a solid block and no item in use to start
    bool breakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block, int32_t face);

    void stopBreakingBlock();

    // sub_1410325B0: the slot selected, then the held item used on the block in view and used again while it
    // stays in view (GameMode::continueBuildBlock, sub_14376BB80)
    void startBuild(ServerNetworkHandler &owner, ServerPlayer &player, int32_t slot);

    // sub_14DD01AB0: GameMode::stopBuildBlock
    void stopBuild() {
        mBuilding = false;
    }

    // sub_141037660: a path from the navigation, then walked; false with no path at all
    bool navigateToLocation(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &target, float speed,
                            std::vector<Vector3i> &path, bool &fullPath);

    // sub_141036D90: the entity followed to within a block, its path made again as it moves away
    bool navigateToEntity(ServerNetworkHandler &owner, ServerPlayer &player, int64_t uniqueId, const Vector3f &target,
                          float speed, std::vector<Vector3i> &path, bool &fullPath);

    // sub_141037990: each location in turn, a path to the next once the last is walked
    void navigateToLocations(const std::vector<Vector3f> &route, float speed);

    // the next input, built from the current orders
    void buildInput(ServerNetworkHandler &owner, ServerPlayer &player, PlayerAuthInputPacket &packet);

    static float wrapDegrees(float angle);

private:
    enum class Move : uint8_t { None, Direction, ToLocation, Navigate };
    enum class Look : uint8_t { None, Location, Entity };
    enum class Request : uint8_t { None, Start, Stop };

    void _faceYaw(ServerPlayer &player, const Vector3f &target);

    void _lookAt(ServerPlayer &player, const Vector3f &target);

    bool _actionReady(ServerNetworkHandler &owner) const;

    void _markAction(ServerNetworkHandler &owner);

    void _breakingActions(ServerNetworkHandler &owner, ServerPlayer &player, PlayerAuthInputPacket &packet);

    bool _buildOn(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block, int32_t face,
                  const Vector3f &clickPosition, bool continued);

    void _continueBuild(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _findPath(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &target);

    // the move vector towards a point, as moveToLocation and the navigation walk it
    void _stepTowards(ServerPlayer &player, const Vector3f &target, float speed, bool faceTarget, float &sideways,
                      float &forward, float &vertical);

    void _tickNavigation(ServerNetworkHandler &owner, ServerPlayer &player, float &sideways, float &forward,
                         float &vertical);

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
    int64_t mJumpTick = -1;
    bool mSprinting = false;
    Request mSwimRequest = Request::None;
    Request mGlideRequest = Request::None;
    int32_t mTestRotation = 0;

    bool mActed = false;
    int64_t mLastActionTick = 0;

    bool mBreaking = false;
    bool mAbortBreak = false;
    Vector3i mBreakBlock;
    int32_t mBreakFace = 1;

    bool mBuilding = false;
    bool mHasBuilt = false;
    std::chrono::steady_clock::time_point mLastBuild;

    std::vector<Vector3f> mWaypoints;
    size_t mWaypoint = 0;
    std::vector<Vector3f> mRoute;
    size_t mRouteIndex = 0;
    int64_t mFollowEntity = 0;
    Vector3f mNavigationTarget;
    Vector3f mLastNavigationPosition;
    int32_t mStuckTicks = 0;
};
