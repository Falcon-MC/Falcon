#include "World/Actor/Movement/Systems/PlayerCollisionSystem.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include <limits>
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Level/Level.h"

#include <cmath>

using namespace MovementPhysics;

namespace MovementPhysics {
    bool stuckMultiplier(Level &level, const Actor &actor, const AxisAlignedBB &box, Vector3f &multiplier) {
        bool stuck = false;
        for (int32_t x = (int32_t) std::floor(box.mMinX); x < (int32_t) std::ceil(box.mMaxX); x++) {
            for (int32_t y = (int32_t) std::floor(box.mMinY); y < (int32_t) std::ceil(box.mMaxY); y++) {
                for (int32_t z = (int32_t) std::floor(box.mMinZ); z < (int32_t) std::ceil(box.mMaxZ); z++) {
                    const Block *block = blockAt(level, (float) x, (float) y, (float) z);
                    Vector3f blockMultiplier;
                    if (block == nullptr || !block->getStuckMultiplier(actor, blockMultiplier))
                        continue;
                    if (!stuck) {
                        multiplier = blockMultiplier;
                        stuck = true;
                        continue;
                    }
                    multiplier.x = std::min(multiplier.x, blockMultiplier.x);
                    multiplier.y = std::min(multiplier.y, blockMultiplier.y);
                    multiplier.z = std::min(multiplier.z, blockMultiplier.z);
                }
            }
        }
        return stuck;
    }

    bool findSupportingBlock(Level &level, const AxisAlignedBB &probe, const Vector3f &feet, Vector3i &found) {
        const float centerX = std::floor(feet.x) + 0.5f;
        const float centerY = std::floor(feet.y) + 0.5f;
        const float centerZ = std::floor(feet.z) + 0.5f;
        float best = std::numeric_limits<float>::max();
        bool any = false;
        for (int32_t x = (int32_t) std::floor(probe.mMinX); x <= (int32_t) std::floor(probe.mMaxX); x++) {
            for (int32_t y = (int32_t) std::floor(probe.mMinY) - 1; y <= (int32_t) std::floor(probe.mMaxY); y++) {
                for (int32_t z = (int32_t) std::floor(probe.mMinZ); z <= (int32_t) std::floor(probe.mMaxZ); z++) {
                    const BlockState *state = level.peekBlockPtr(x, y, z);
                    if (state == nullptr || !BlockShape::hasCollision(*state))
                        continue;
                    if (!BlockShape::getShapeAt(*state, x, y, z).intersectsWith(probe))
                        continue;

                    const float dx = (float) x - centerX;
                    const float dy = (float) y - centerY;
                    const float dz = (float) z - centerZ;
                    const float distance = dx * dx + dy * dy + dz * dz;
                    if (distance < best) {
                        best = distance;
                        found = Vector3i(x, y, z);
                        any = true;
                    }
                }
            }
        }
        return any;
    }

    const BlockBehavior &behaviorAt(Level &level, int32_t x, int32_t y, int32_t z) {
        static const BlockBehavior air;
        const BlockState *state = level.peekBlockPtr(x, y, z);
        return state == nullptr ? air : Block(*state).getBehavior();
    }

    const BlockBehavior &behaviorUnder(Level &level, const Vector3f &feet, const ServerPlayer &player) {
        if (player.hasSupportingBlock()) {
            const Vector3i &support = player.getSupportingBlock();
            return behaviorAt(level, support.x, support.y, support.z);
        }

        const int32_t x = (int32_t) std::floor(feet.x);
        const int32_t y = (int32_t) std::floor(feet.y - SUPPORT_PROBE_DEPTH);
        const int32_t z = (int32_t) std::floor(feet.z);
        const BlockState *under = level.peekBlockPtr(x, y, z);
        if (under == nullptr || BlockShape::hasCollision(*under))
            return behaviorAt(level, x, y, z);

        const int32_t belowY = (int32_t) std::floor(feet.y) - 1;
        const BlockState *below = level.peekBlockPtr(x, belowY, z);
        if (below != nullptr && BlockShape::hasCollision(*below)
            && BlockShape::getShapeAt(*below, x, belowY, z).mMaxY > (float) belowY + 1.0f)
            return behaviorAt(level, x, belowY, z);
        return behaviorAt(level, x, y, z);
    }

    void updateSupportingBlock(Level &level, ServerPlayer &player, const AxisAlignedBB &box, const Vector3f &feet,
                               bool onGround, const Vector3f &requested) {
        if (!onGround) {
            player.clearSupportingBlock();
            return;
        }

        AxisAlignedBB probe = box;
        probe.mMinY -= SUPPORT_PROBE_MARGIN;
        Vector3i found;
        if (findSupportingBlock(level, probe, feet, found)
            || findSupportingBlock(level, probe.offset(-requested.x, 0.0f, -requested.z), feet, found)) {
            player.setSupportingBlock(found);
            return;
        }
        player.clearSupportingBlock();
    }

    float landingVelocity(const BlockBehavior &under, float velocityY) {
        const float bounced = -under.getLandingBounce() * velocityY;
        return std::fabs(bounced) < MIN_BOUNCE_VELOCITY ? 0.0f : bounced;
    }

    float reduceTowardEdge(float value) {
        if (value < EDGE_STEP && value >= -EDGE_STEP)
            return 0.0f;
        return value > 0.0f ? value - EDGE_STEP : value + EDGE_STEP;
    }

    /** Sneaking shortens each axis, then both together, until support remains under the box. */
    void avoidEdge(Level &level, const AxisAlignedBB &box, Vector3f &velocity) {
        const AxisAlignedBB support = box.expand(-EDGE_INSET, 0.0f, -EDGE_INSET);
        const float drop = -STEP_HEIGHT * 1.01f;
        float x = velocity.x;
        float z = velocity.z;

        int32_t iterations = 0;
        while (x != 0.0f && !collides(level, support.offset(x, drop, 0.0f))) {
            x = ++iterations >= EDGE_MAX_ITERATIONS ? 0.0f : reduceTowardEdge(x);
        }

        iterations = 0;
        while (z != 0.0f && !collides(level, support.offset(0.0f, drop, z))) {
            z = ++iterations >= EDGE_MAX_ITERATIONS ? 0.0f : reduceTowardEdge(z);
        }

        iterations = 0;
        while (x != 0.0f && z != 0.0f && !collides(level, support.offset(x, drop, z))) {
            if (++iterations >= EDGE_MAX_ITERATIONS) {
                x = 0.0f;
                z = 0.0f;
                break;
            }
            x = reduceTowardEdge(x);
            z = reduceTowardEdge(z);
        }

        velocity.x = x;
        velocity.z = z;
    }
}

void PlayerCollisionSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &travel = context.travel;

    travel.box = boxAt(state.start, travel.pose.mHeight);
    Vector3f stuck;
    travel.isStuck = !abilities.spectator && stuckMultiplier(context.level, context.player, travel.box, stuck);
    if (travel.isStuck) {
        state.velocity.x *= stuck.x;
        state.velocity.y *= stuck.y;
        state.velocity.z *= stuck.z;
    }

    if (!abilities.flying && travel.sneaking && !travel.crawling && state.onGround && state.velocity.y <= 0.0f)
        avoidEdge(context.level, travel.box, state.velocity);

    if (abilities.spectator) {
        travel.box = travel.box.offset(state.velocity.x, state.velocity.y, state.velocity.z);
        travel.moved.mMoved = state.velocity;
    } else {
        travel.moved = ActorCollisionSystem::move(context.level, travel.box, state.velocity, STEP_HEIGHT, state.onGround);
    }
    state.simulated = Vector3f(travel.box.mMinX + PLAYER_WIDTH * 0.5f, travel.box.mMinY, travel.box.mMinZ + PLAYER_WIDTH * 0.5f);
    const bool wasOnGround = state.onGround;
    state.onGround = travel.moved.mOnGround;

    updateSupportingBlock(context.level, context.player, travel.box, state.simulated, travel.moved.mOnGround, state.velocity);
    const BlockBehavior &under = behaviorUnder(context.level, state.simulated, context.player);
    if (state.simulated.y == state.start.y && state.onGround && !travel.sneaking && !input.pressingSneak && under.slowsWalking()) {
        const float vertical = std::fabs(travel.moved.mMoved.y);
        if (vertical < WALK_SLOWDOWN_LIMIT) {
            const float slowdown = WALK_SLOWDOWN_BASE + vertical * WALK_SLOWDOWN_PER_VELOCITY;
            state.velocity.x *= slowdown;
            state.velocity.z *= slowdown;
        }
    }

    if (travel.isStuck)
        state.velocity = Vector3f(0.0f, 0.0f, 0.0f);
    context.player.setHorizontalCollision(travel.moved.mCollidedX || travel.moved.mCollidedZ);

    if (travel.moved.mCollidedX)
        state.velocity.x = 0.0f;
    if (travel.moved.mCollidedZ)
        state.velocity.z = 0.0f;
    if (travel.moved.mCollidedY)
        state.velocity.y = wasOnGround || state.velocity.y >= 0.0f || input.pressingSneak ? 0.0f : landingVelocity(under, state.velocity.y);
}
