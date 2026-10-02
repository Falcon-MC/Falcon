#include "Actor/Movement/PlayerMovementSimulator.h"

#include "Actor/Movement/ActorCollisionSystem.h"
#include "Actor/Movement/PhysicsComponent.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Components/BlockBehavior.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Inventory/PlayerInventory.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Level/Level.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>

namespace {
    const float AIR_FRICTION = 0.91f;
    const float GRAVITY = 0.08f;
    const float SLOW_FALLING_GRAVITY = 0.01f;
    const float GRAVITY_MULTIPLIER = 0.98f;
    const float STEP_HEIGHT = 0.5625f;
    const float SNEAK_INPUT = 0.3f;
    const float CONSUMING_INPUT = 0.1225f;
    const float WALK_AIR_SPEED = 0.02f;
    const float SPRINT_AIR_SPEED = 0.026f;
    const float DEFAULT_MOVE_SPEED = 0.1f;
    const float DEFAULT_BLOCK_FRICTION = 0.6f;
    const float IMPULSE_SCALE = 0.98f;
    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float PLAYER_SNEAKING_HEIGHT = 1.49f;
    const float PLAYER_CRAWLING_HEIGHT = 0.6f;
    const int32_t JUMP_DELAY_TICKS = 10;
    const float EDGE_INSET = 0.025f;
    const float EDGE_STEP = 0.05f;
    const int32_t EDGE_MAX_ITERATIONS = 1000;
    const float SUPPORT_PROBE_DEPTH = 0.2f;
    const float MIN_BOUNCE_VELOCITY = 1.0e-4f;
    const float WALK_SLOWDOWN_LIMIT = 0.1f;
    const float WALK_SLOWDOWN_BASE = 0.4f;
    const float WALK_SLOWDOWN_PER_VELOCITY = 0.2f;
    const float GROUND_PROBE_DEPTH = 0.5f;
    const float JUMP_PROBE_DEPTH = 0.1f;
    const float JUMP_VELOCITY = 0.42f;
    const float CLIMB_SPEED = 0.2f;
    const float TRAVERSAL_SPEED = 0.15f;
    const float POWDER_SNOW_ASCEND_SPEED = 0.2f;
    const float HONEY_CONTACT_MARGIN = 1.0e-3f;
    const float HONEY_HORIZONTAL_SLIDE = 0.4f;
    const float HONEY_MAX_FALL = -0.12f;
    const char *const LEATHER_BOOTS = "minecraft:leather_boots";
    const float MOJANG_PI = 3.1415927f;
    const float SIN_COEFFICIENTS[] = {
            1.5896230e-10f, -2.5050748e-8f, 2.7557314e-6f, -1.9841270e-4f, 8.333334e-3f, -1.6666667e-1f
    };
    const float COS_COEFFICIENTS[] = {
            -1.1358537e-11f, 2.0875701e-9f, -2.7557314e-7f, 2.4801588e-5f, -1.3888889e-3f, 4.1666668e-2f
    };

    float mojangFloatSin(float value) {
        if (value == 0.0f || std::isnan(value))
            return value;
        if (std::isinf(value))
            return NAN;

        bool negative = false;
        if (value < 0.0f) {
            value = -value;
            negative = true;
        }

        long long octant = (long long) (value * (4.0f / MOJANG_PI));
        float octantValue = (float) octant;
        if ((octant & 1LL) == 1LL) {
            octant++;
            octantValue++;
        }
        octant &= 7LL;
        float reduced = ((value - octantValue * 0.7853981f) - octantValue * 3.7748947e-8f)
                        - octantValue * 2.6951514e-15f;
        if (octant > 3LL) {
            negative = !negative;
            octant -= 4LL;
        }

        const float squared = reduced * reduced;
        float result;
        if (octant == 1LL || octant == 2LL) {
            result = 1.0f - 0.5f * squared + squared * squared
                     * (((((COS_COEFFICIENTS[0] * squared + COS_COEFFICIENTS[1]) * squared + COS_COEFFICIENTS[2])
                          * squared + COS_COEFFICIENTS[3]) * squared + COS_COEFFICIENTS[4]) * squared
                        + COS_COEFFICIENTS[5]);
        } else {
            result = reduced + reduced * squared
                     * (((((SIN_COEFFICIENTS[0] * squared + SIN_COEFFICIENTS[1]) * squared + SIN_COEFFICIENTS[2])
                          * squared + SIN_COEFFICIENTS[3]) * squared + SIN_COEFFICIENTS[4]) * squared
                        + SIN_COEFFICIENTS[5]);
        }
        return negative ? -result : result;
    }

    const float *mojangSineTable() {
        static float table[65536];
        static bool ready = false;
        if (!ready) {
            for (int index = 0; index < 65536; index++) {
                const float angle = ((float) index * MOJANG_PI * 2.0f) / 65536.0f;
                table[index] = mojangFloatSin(angle);
            }
            ready = true;
        }
        return table;
    }

    float mojangSin(float radians) {
        return mojangSineTable()[(int) (radians * 10430.378f) & 65535];
    }

    float mojangCos(float radians) {
        return mojangSineTable()[(int) (radians * 10430.378f + 16384.0f) & 65535];
    }

    bool chunkLoaded(Level &level, const Vector3f &position) {
        return level.getBlockActors().isChunkLoaded((int32_t) std::floor(position.x) >> 4,
                                                    (int32_t) std::floor(position.z) >> 4);
    }

    AxisAlignedBB boxAt(const Vector3f &feet, float height) {
        const float half = PLAYER_WIDTH * 0.5f;
        return AxisAlignedBB(feet.x - half, feet.y, feet.z - half, feet.x + half, feet.y + height, feet.z + half);
    }

    struct PlayerPose {
        bool mSneaking = false;
        bool mCrawling = false;
        float mHeight = PLAYER_HEIGHT;
    };

    bool collides(Level &level, const AxisAlignedBB &box) {
        for (const AxisAlignedBB &obstacle: level.getCollisionBoxes(box)) {
            if (obstacle.intersectsWith(box))
                return true;
        }
        return false;
    }

    bool fits(Level &level, const Vector3f &feet, float height) {
        return !collides(level, boxAt(feet, height));
    }

    PlayerPose resolvePose(Level &level, const Vector3f &feet, const PlayerAuthInputPacket &packet,
                           bool wasSneaking, bool wasCrawling) {
        const bool sneakDown = packet.hasInputFlag((int32_t) PlayerAuthInputData::Sneaking);
        const bool startSneaking = packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSneaking);
        const bool stopSneaking = packet.hasInputFlag((int32_t) PlayerAuthInputData::StopSneaking);
        const bool wantSneak = (sneakDown || startSneaking) && !stopSneaking;

        PlayerPose pose;
        pose.mSneaking = wasSneaking;
        pose.mCrawling = wasCrawling;
        if (startSneaking) {
            pose.mSneaking = true;
        } else if (stopSneaking) {
            pose.mSneaking = !pose.mCrawling && !fits(level, feet, PLAYER_HEIGHT);
        } else if (pose.mCrawling) {
            pose.mSneaking = false;
        } else if (sneakDown) {
            pose.mSneaking = true;
        } else {
            pose.mSneaking = pose.mSneaking && !fits(level, feet, PLAYER_HEIGHT);
        }

        if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StartCrawling)) {
            if (!fits(level, feet, PLAYER_HEIGHT)) {
                pose.mCrawling = true;
                pose.mSneaking = false;
            }
        } else if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StopCrawling)) {
            const float target = wantSneak ? PLAYER_SNEAKING_HEIGHT : PLAYER_HEIGHT;
            if (fits(level, feet, target)) {
                pose.mCrawling = false;
                pose.mSneaking = wantSneak;
            }
        }

        if (pose.mCrawling)
            pose.mHeight = PLAYER_CRAWLING_HEIGHT;
        else if (pose.mSneaking)
            pose.mHeight = PLAYER_SNEAKING_HEIGHT;
        return pose;
    }

    const Block *blockAt(Level &level, float x, float y, float z) {
        const BlockState *state = level.peekBlockPtr((int32_t) std::floor(x), (int32_t) std::floor(y),
                                                     (int32_t) std::floor(z));
        return state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
    }

    float blockFriction(const Block *ground) {
        if (ground == nullptr)
            return DEFAULT_BLOCK_FRICTION;
        const float friction = ground->getFrictionFactor();
        return friction > 0.0f ? friction : DEFAULT_BLOCK_FRICTION;
    }

    bool stuckMultiplier(Level &level, const AxisAlignedBB &box, Vector3f &multiplier) {
        bool stuck = false;
        for (int32_t x = (int32_t) std::floor(box.mMinX); x < (int32_t) std::ceil(box.mMaxX); x++) {
            for (int32_t y = (int32_t) std::floor(box.mMinY); y < (int32_t) std::ceil(box.mMaxY); y++) {
                for (int32_t z = (int32_t) std::floor(box.mMinZ); z < (int32_t) std::ceil(box.mMaxZ); z++) {
                    const Block *block = blockAt(level, (float) x, (float) y, (float) z);
                    Vector3f blockMultiplier;
                    if (block == nullptr || !block->getStuckMultiplier(blockMultiplier))
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

    void slideAlongHoney(Level &level, const AxisAlignedBB &box, Vector3f &velocity) {
        const AxisAlignedBB contact = box.expand(HONEY_CONTACT_MARGIN, 0.0f, HONEY_CONTACT_MARGIN);
        for (int32_t x = (int32_t) std::floor(contact.mMinX); x < (int32_t) std::ceil(contact.mMaxX); x++) {
            for (int32_t y = (int32_t) std::floor(contact.mMinY); y < (int32_t) std::ceil(contact.mMaxY); y++) {
                for (int32_t z = (int32_t) std::floor(contact.mMinZ); z < (int32_t) std::ceil(contact.mMaxZ); z++) {
                    const Block *block = blockAt(level, (float) x, (float) y, (float) z);
                    if (block == nullptr || !block->getBehavior().slidesAlongSides())
                        continue;
                    velocity.x *= HONEY_HORIZONTAL_SLIDE;
                    velocity.y = std::max(HONEY_MAX_FALL, velocity.y);
                    velocity.z *= HONEY_HORIZONTAL_SLIDE;
                }
            }
        }
    }

    float jumpFactor(Level &level, const Vector3f &feet) {
        const Block *inside = blockAt(level, feet.x, feet.y, feet.z);
        if (inside != nullptr && inside->getJumpFactor() != 1.0f)
            return inside->getJumpFactor();
        const Block *below = blockAt(level, feet.x, feet.y - JUMP_PROBE_DEPTH, feet.z);
        return below == nullptr ? 1.0f : below->getJumpFactor();
    }

    const BlockBehavior &behaviorUnder(Level &level, const Vector3f &feet) {
        static const BlockBehavior air;
        const BlockState *below = level.peekBlockPtr((int32_t) std::floor(feet.x),
                                                     (int32_t) std::floor(feet.y - SUPPORT_PROBE_DEPTH),
                                                     (int32_t) std::floor(feet.z));
        return below == nullptr ? air : Block(*below).getBehavior();
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

    void moveRelative(Vector3f &velocity, float sideways, float forward, float speed, float yawDegrees) {
        float force = sideways * sideways + forward * forward;
        if (force < 1.0e-4f)
            return;
        force = speed / std::max(std::sqrt(force), 1.0f);
        sideways *= force;
        forward *= force;
        const float yaw = yawDegrees * MOJANG_PI / 180.0f;
        const float sine = mojangSin(yaw);
        const float cosine = mojangCos(yaw);
        velocity.x += sideways * cosine - forward * sine;
        velocity.z += forward * cosine + sideways * sine;
    }
}

void PlayerMovementSimulator::apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                    const PlayerAuthInputPacket &packet, Vector3f &feetPosition) {
    const int32_t gameType = player.getGameType();
    const bool skip = gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator
                      || player.isRiding() || player.isFlying() || player.isSleeping() || player.hasPendingMovementChange()
                      || player.hasEffect(MobEffectId::Levitation)
                      || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartGliding)
                      || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSwimming)
                      || packet.hasInputFlag((int32_t) PlayerAuthInputData::InClientPredictedInVehicle)
                      || packet.hasInputFlag((int32_t) PlayerAuthInputData::HandleTeleport);
    Level &level = owner.getLevelFor(player);
    const LiquidContact liquid = LiquidBlocksFetch::at(level, player.getPosition());
    if (skip || liquid.water || liquid.lava || !chunkLoaded(level, player.getPosition())
        || !chunkLoaded(level, feetPosition)) {
        player.clearMovementSimulation();
        return;
    }

    const Vector3f start = player.hasSimulatedPosition() ? player.getSimulatedPosition() : player.getPosition();
    Vector3f velocity;
    if (!player.takeKnockback(velocity))
        velocity = player.hasSimulatedVelocity() ? player.getSimulatedVelocity() : packet.mDelta;
    bool onGround = player.isOnGround();
    int32_t jumpDelay = player.getJumpDelay();
    const bool jumpHeld = packet.hasInputFlag((int32_t) PlayerAuthInputData::Jumping)
                          || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartJumping);
    const bool startJumping = packet.hasInputFlag((int32_t) PlayerAuthInputData::StartJumping);
    if (!jumpHeld)
        jumpDelay = 0;

    const bool sprinting = packet.hasInputFlag((int32_t) PlayerAuthInputData::Sprinting)
                           || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSprinting);
    const PlayerPose pose = resolvePose(level, start, packet, player.isSimulatedSneaking(),
                                        player.isSimulatedCrawling());
    player.setSimulatedPose(pose.mSneaking, pose.mCrawling);
    const bool sneaking = pose.mSneaking;
    const bool crawling = pose.mCrawling;

    float maxImpulse = 1.0f;
    if (player.getFlags().get(ActorFlag::UsingItem))
        maxImpulse *= CONSUMING_INPUT;
    if (sneaking || crawling)
        maxImpulse *= SNEAK_INPUT;
    const float sideways = std::clamp(packet.mMotionX, -maxImpulse, maxImpulse) * IMPULSE_SCALE;
    const float forward = std::clamp(packet.mMotionY, -maxImpulse, maxImpulse) * IMPULSE_SCALE;

    const ItemStack &boots = player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    float friction = AIR_FRICTION;
    float acceleration = sprinting ? SPRINT_AIR_SPEED : WALK_AIR_SPEED;
    if (onGround) {
        const Block *ground = blockAt(level, start.x, start.y - GROUND_PROBE_DEPTH, start.z);
        const float groundFriction = blockFriction(ground);
        friction *= groundFriction;
        const float speed = std::max(0.0f, player.getAttributes().get("minecraft:movement"));
        float accelerationMultiplier = ground == nullptr ? 1.0f : ground->getAccelerationFrictionMultiplier();
        if (ItemEnchantments::getLevel(boots, EnchantmentIds::SOUL_SPEED) > 0)
            accelerationMultiplier = 1.0f;
        const float accelerationFriction = (groundFriction * accelerationMultiplier) * AIR_FRICTION;
        const float baseFriction = AIR_FRICTION * DEFAULT_BLOCK_FRICTION;
        const float ratio = baseFriction / accelerationFriction;
        const float movement = speed > 0.0f ? speed : DEFAULT_MOVE_SPEED;
        acceleration = ((movement * ratio) * ratio) * ratio;
    }

    moveRelative(velocity, sideways, forward, acceleration, packet.mRotation.y);

    if (startJumping && onGround && jumpDelay <= 0) {
        const float jumpVelocity = JUMP_VELOCITY * jumpFactor(level, start);
        velocity.y = std::max(jumpVelocity * player.getEffects().jumpVelocityMultiplier(), velocity.y);
        jumpDelay = JUMP_DELAY_TICKS;
        if (sprinting) {
            const float direction = packet.mRotation.y * 0.017453292f;
            velocity.x -= mojangSin(direction) * 0.2f;
            velocity.z += mojangCos(direction) * 0.2f;
        }
    }

    const Block *inside = blockAt(level, start.x, start.y, start.z);
    const BlockTraversal traversal = inside == nullptr ? BlockTraversal::None : inside->getTraversal();
    bool scaffoldDescend = false;
    if (traversal == BlockTraversal::Scaffolding) {
        if (sneaking) {
            velocity.y = -TRAVERSAL_SPEED;
            scaffoldDescend = true;
        } else if (jumpHeld) {
            velocity.y = TRAVERSAL_SPEED;
        }
    } else if (traversal == BlockTraversal::PowderSnow) {
        if (sneaking)
            velocity.y = -TRAVERSAL_SPEED;
        else if (jumpHeld && !boots.isAir() && boots.mDefinition->getIdentifier() == LEATHER_BOOTS)
            velocity.y = POWDER_SNOW_ASCEND_SPEED;
    }

    if (inside != nullptr && inside->isClimbable() && traversal == BlockTraversal::None) {
        velocity.y = std::max(velocity.y, -CLIMB_SPEED);
        if (jumpHeld || player.hasHorizontalCollision())
            velocity.y = CLIMB_SPEED;
        if (sneaking && velocity.y < 0.0f)
            velocity.y = 0.0f;
    }

    AxisAlignedBB box = boxAt(start, pose.mHeight);
    Vector3f stuck;
    const bool isStuck = stuckMultiplier(level, box, stuck);
    if (isStuck) {
        velocity.x *= stuck.x;
        velocity.y *= stuck.y;
        velocity.z *= stuck.z;
    }

    if (sneaking && !crawling && onGround && velocity.y <= 0.0f)
        avoidEdge(level, box, velocity);

    const ActorMoveResult moved = ActorCollisionSystem::move(level, box, velocity, STEP_HEIGHT, onGround);
    Vector3f simulated(box.mMinX + PLAYER_WIDTH * 0.5f, box.mMinY, box.mMinZ + PLAYER_WIDTH * 0.5f);
    const bool wasOnGround = onGround;
    onGround = moved.mOnGround;

    const BlockBehavior &under = behaviorUnder(level, simulated);
    if (simulated.y == start.y && onGround && !sneaking && under.slowsWalking()) {
        const float vertical = std::fabs(moved.mMoved.y);
        if (vertical < WALK_SLOWDOWN_LIMIT) {
            const float slowdown = WALK_SLOWDOWN_BASE + vertical * WALK_SLOWDOWN_PER_VELOCITY;
            velocity.x *= slowdown;
            velocity.z *= slowdown;
        }
    }

    if (isStuck)
        velocity = Vector3f(0.0f, 0.0f, 0.0f);
    player.setHorizontalCollision(moved.mCollidedX || moved.mCollidedZ);

    if (moved.mCollidedX)
        velocity.x = 0.0f;
    if (moved.mCollidedZ)
        velocity.z = 0.0f;
    if (moved.mCollidedY)
        velocity.y = wasOnGround || velocity.y >= 0.0f || sneaking ? 0.0f : landingVelocity(under, velocity.y);

    if (!scaffoldDescend) {
        const float gravity = player.hasEffect(MobEffectId::SlowFalling) && velocity.y < 0.0f ? SLOW_FALLING_GRAVITY
                                                                                              : GRAVITY;
        velocity.y = (velocity.y - gravity) * GRAVITY_MULTIPLIER;
    }
    velocity.x *= friction;
    velocity.z *= friction;
    slideAlongHoney(level, box, velocity);

    if (jumpDelay > 0)
        jumpDelay -= 1;
    player.setJumpDelay(jumpDelay);

    const float dx = feetPosition.x - simulated.x;
    const float dy = feetPosition.y - simulated.y;
    const float dz = feetPosition.z - simulated.z;
    const float threshold = std::max(0.0f, owner.getProperties().getPlayerPositionAcceptanceThreshold());
    if (dx * dx + dy * dy + dz * dz <= threshold * threshold) {
        player.setSimulatedPosition(feetPosition);
        player.setSimulatedVelocity(packet.mDelta);
        return;
    }

    player.setSimulatedPosition(simulated);
    player.setSimulatedVelocity(velocity);
    feetPosition = simulated;
    player.setMotion(velocity);

    CorrectPlayerMovePredictionPacket correction;
    correction.mPredictionType = PredictionType::Player;
    correction.mPosition = Vector3f(simulated.x, simulated.y + PLAYER_BASE_OFFSET, simulated.z);
    correction.mDelta = velocity;
    correction.mVehicleRotation = Vector2f(packet.mRotation.x, packet.mRotation.y);
    correction.mHasVehicleAngularVelocity = false;
    correction.mOnGround = onGround;
    correction.mTick = (uint64_t) packet.mTick;
    owner.getNetworkHandler().send(id, correction, owner.getCodecContext());
}
