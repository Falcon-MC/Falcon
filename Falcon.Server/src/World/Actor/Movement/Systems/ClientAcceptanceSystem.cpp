#include "World/Actor/Movement/Systems/ClientAcceptanceSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

using namespace MovementPhysics;

void ClientAcceptanceSystem::tick(PlayerMovementContext &context) {
    auto &acceptance = context.prediction.validator.clientAcceptance();
    auto &state = context.state;
    auto &travel = context.travel;
    if (acceptance.mCooldown != 0) {
        --acceptance.mCooldown;
        return;
    }
    if (context.abilities.spectator || travel.isStuck)
        return;

    // BDS ClientAcceptanceSystem (0x141038FF0) selects the solid-shape thresholds
    // when the move's collision list contains block shapes. Defaults: 0x1402C1190.
    const AxisAlignedBB sweep = boxAt(state.start, travel.pose.mHeight).addCoord(
            travel.moved.mRequested.x, travel.moved.mRequested.y, travel.moved.mRequested.z);
    const bool solid = !context.level.getCollisionBoxes(sweep).empty();
    const float refill = solid ? 0.1f : 0.035f;
    const float maximum = solid ? 1.0f : 0.5f;
    const float verticalLimit = solid ? 1.0f : 0.43f;
    const float horizontalLimit = solid ? 0.75f : 0.1f;
    const float minimum = 0.05f;
    float remaining = acceptance.mBudget;
    Vector3f accepted;
    const Vector3f error(context.prediction.feetPosition.x - state.simulated.x,
                         context.prediction.feetPosition.y - state.simulated.y,
                         context.prediction.feetPosition.z - state.simulated.z);
    // BDS spends the shared Manhattan-distance budget in Y, X, Z order, accepting only upward Y.
    if (error.y > minimum && error.y < std::min(verticalLimit, remaining)) {
        accepted.y = error.y;
        remaining -= error.y;
    }
    if (std::fabs(error.x) > minimum && std::fabs(error.x) < std::min(horizontalLimit, remaining)) {
        accepted.x = error.x;
        remaining -= std::fabs(error.x);
    }
    if (std::fabs(error.z) > minimum && std::fabs(error.z) < std::min(horizontalLimit, remaining)) {
        accepted.z = error.z;
        remaining -= std::fabs(error.z);
    }
    const float spent = std::fabs(accepted.x) + std::fabs(accepted.y) + std::fabs(accepted.z);
    if (spent > minimum) {
        // 0x141038C40 collision-tests acceptance separately; it does not replace physical velocity.
        const auto moved = ActorCollisionSystem::move(context.level, travel.box, accepted, 0.0f, state.onGround);
        travel.accepted = moved.mMoved;
        travel.moved.mMoved.x += moved.mMoved.x;
        travel.moved.mMoved.y += moved.mMoved.y;
        travel.moved.mMoved.z += moved.mMoved.z;
        state.simulated = Vector3f(travel.box.mMinX + PLAYER_WIDTH * 0.5f, travel.box.mMinY,
                                   travel.box.mMinZ + PLAYER_WIDTH * 0.5f);
        const bool horizontalCollision = input(context.input.packet, PlayerAuthInputData::HorizontalCollision);
        const bool verticalCollision = input(context.input.packet, PlayerAuthInputData::VerticalCollision);
        if (!horizontalCollision) {
            travel.moved.mRequested.x += accepted.x;
            travel.moved.mRequested.z += accepted.z;
        }
        if (!verticalCollision)
            travel.moved.mRequested.y += accepted.y;
        // BDS 0x141038C40 snaps sub-0.0001 collision residuals before finalizing the move.
        const auto snap = [](float requested, float &actual) {
            if (std::fabs(requested - actual) <= 0.0001f)
                actual = requested;
        };
        snap(travel.moved.mRequested.x, travel.moved.mMoved.x);
        snap(travel.moved.mRequested.y, travel.moved.mMoved.y);
        snap(travel.moved.mRequested.z, travel.moved.mMoved.z);
        travel.moved.mCollidedX = travel.moved.mRequested.x != travel.moved.mMoved.x;
        travel.moved.mCollidedY = travel.moved.mRequested.y != travel.moved.mMoved.y;
        travel.moved.mCollidedZ = travel.moved.mRequested.z != travel.moved.mMoved.z;
        state.onGround = (travel.moved.mCollidedY && travel.moved.mRequested.y < 0.0f)
                         || (state.onGround && !travel.moved.mCollidedY && travel.moved.mRequested.y == 0.0f);
        travel.moved.mOnGround = state.onGround;
        acceptance.mBudget -= spent;
    }
    acceptance.mBudget = std::min(acceptance.mBudget + refill, maximum);
}
