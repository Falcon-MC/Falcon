#include "World/Actor/Movement/PlayerMovementSimulator.h"

#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "World/Actor/Movement/PhysicsComponent.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ElytraItem.h"
#include "World/Level/Level.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    const float AIR_FRICTION = 0.91f;
    const float GRAVITY = 0.08f;
    const float SLOW_FALLING_GRAVITY = 0.01f;
    const float GRAVITY_MULTIPLIER = 0.98f;
    const float STEP_HEIGHT = 0.5625f;
    const float SNEAK_INPUT = 0.3f;
    const float SWIFT_SNEAK_PER_LEVEL = 0.15f;
    const int32_t SWIFT_SNEAK_DELAY_TICKS = 2;
    const float JUMP_BOOST_PER_LEVEL = 0.1f;
    const float CONSUMING_INPUT = 0.1225f;
    const float WALK_AIR_SPEED = 0.02f;
    const float SPRINT_AIR_SPEED = 0.026f;
    const float SPRINT_SPEED_MULTIPLIER = 1.3f;
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
    const float SUPPORT_PROBE_MARGIN = 1.0e-3f;
    const float MIN_BOUNCE_VELOCITY = 1.0e-4f;
    const float WALK_SLOWDOWN_LIMIT = 0.1f;
    const float WALK_SLOWDOWN_BASE = 0.4f;
    const float WALK_SLOWDOWN_PER_VELOCITY = 0.2f;
    const float GROUND_PROBE_DEPTH = 0.1f;
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

    /**
     * Single-precision sine with the client's own range reduction and polynomial. The result
     * feeds the sine table below, so it has to match the client bit for bit or simulated
     * positions drift away from the client's prediction and trigger needless corrections.
     */
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

    /**
     * The client looks movement trigonometry up in a 65536-entry table rather than calling
     * sin/cos; 10430.378 is 65536 / 2pi, and cosine is the same table shifted a quarter turn.
     */
    float mojangSin(float radians) {
        return mojangSineTable()[(int) (radians * 10430.378f) & 65535];
    }

    float mojangCos(float radians) {
        return mojangSineTable()[(int) (radians * 10430.378f + 16384.0f) & 65535];
    }

    bool input(const PlayerAuthInputPacket &packet, PlayerAuthInputData flag) {
        return packet.hasInputFlag((int32_t) flag);
    }

    bool activeState(bool previous, const PlayerAuthInputPacket &packet,
                     PlayerAuthInputData start, PlayerAuthInputData stop) {
        return (previous || input(packet, start)) && !input(packet, stop);
    }

    // BDS GlideMoveSystem (0x140C66F70), before collision resolution.
    void glide(Vector3f &velocity, const PlayerAuthInputPacket &packet, bool slowFalling) {
        const float pitch = packet.mRotation.x * MOJANG_PI / 180.0f;
        const float yaw = packet.mRotation.y * MOJANG_PI / 180.0f;
        const float cosine = mojangCos(pitch);
        const Vector3f look(-mojangSin(yaw) * cosine, -mojangSin(pitch), mojangCos(yaw) * cosine);
        const float horizontalLook = std::sqrt(look.x * look.x + look.z * look.z);
        const float horizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        const float lift = std::min(std::sqrt(look.x * look.x + look.y * look.y + look.z * look.z) * 2.5f, 1.0f)
                           * cosine * cosine;
        velocity.y -= (slowFalling ? -0.0075f : -0.06f) * lift - (slowFalling ? -0.01f : -0.08f);
        if (velocity.y < 0.0f && horizontalLook > 0.0f) {
            const float fallLift = velocity.y * -0.1f * lift;
            velocity.x += fallLift * look.x * (1.0f / horizontalLook);
            velocity.y += fallLift;
            velocity.z += fallLift * look.z * (1.0f / horizontalLook);
        }
        if (pitch < 0.0f && horizontalLook > 0.0f) {
            const float climb = horizontalSpeed * mojangSin(pitch) * -0.04f;
            velocity.x -= climb * look.x * (1.0f / horizontalLook);
            velocity.y += climb * 3.2f;
            velocity.z -= climb * look.z * (1.0f / horizontalLook);
        }
        if (horizontalLook > 0.0f) {
            velocity.x += (look.x * (1.0f / horizontalLook) * horizontalSpeed - velocity.x) * 0.1f;
            velocity.z += (look.z * (1.0f / horizontalLook) * horizontalSpeed - velocity.z) * 0.1f;
        }
        velocity.x *= 0.99f;
        velocity.y *= 0.98f;
        velocity.z *= 0.99f;
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
        const bool sneakDown = packet.hasInputFlag((int32_t) PlayerAuthInputData::SneakDown);
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

    /**
     * Sneaking keeps the player on the block: each horizontal axis, then both together, is
     * shortened in 0.05 steps until the box would still be standing on something after the
     * move. The iteration cap guards against a pathological loop.
     */
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
                                    const PlayerAuthInputPacket &packet, Vector3f &feetPosition, bool sprinting) {
    Level &level = owner.getLevelFor(player);
    const Vector3f start = player.hasSimulatedPosition() ? player.getSimulatedPosition() : player.getPosition();
    const ItemStack &boots = player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    const Block *soulSupport = blockAt(level, start.x, start.y - SUPPORT_PROBE_DEPTH, start.z);
    // BDS keeps the modifier over air while jumping, and refreshes it on the next non-air block.
    if (soulSupport != nullptr && soulSupport->getTypeId() != BlockTypeIds::AIR) {
        const int enchantmentLevel = ItemEnchantments::getLevel(boots, EnchantmentIds::SOUL_SPEED);
        const float bonus = enchantmentLevel > 0 && soulSupport->getBehavior().supportsSoulSpeed()
                            ? 0.03f + 0.0105f * (float) enchantmentLevel : 0.0f;
        player.getEffects().setMovementSpeedBonus(bonus);
    }
    if (!owner.getProperties().getSimulation()) {
        player.clearMovementSimulation();
        return;
    }

    const int32_t gameType = player.getGameType();
    const bool spectator = gameType == (int32_t) GameType::Spectator;
    const bool flying = player.isFlying() || spectator;
    const bool gliding = !flying && ElytraItem::isWearingElytra(player)
                         && activeState(player.getFlags().get(ActorFlag::Gliding), packet,
                                        PlayerAuthInputData::StartGliding, PlayerAuthInputData::StopGliding);
    const bool swimming = !flying && activeState(player.getFlags().get(ActorFlag::Swimming), packet,
                                                PlayerAuthInputData::StartSwimming, PlayerAuthInputData::StopSwimming);
    const bool skip = player.isRiding() || player.isSleeping() || player.hasPendingMovementChange()
                      || packet.hasInputFlag((int32_t) PlayerAuthInputData::InClientPredictedInVehicle)
                      || packet.hasInputFlag((int32_t) PlayerAuthInputData::HandleTeleport);
    const LiquidContact liquid = LiquidBlocksFetch::at(level, start);
    const bool waterTravel = !flying && liquid.water;
    const bool lavaTravel = !flying && !waterTravel && liquid.lava;
    if (skip || !chunkLoaded(level, start)
        || !chunkLoaded(level, feetPosition)) {
        player.clearMovementSimulation();
        return;
    }

    const bool strict = owner.getProperties().getServerAuthoritativeMovementStrict();
    Vector3f velocity;
    if (!player.takeKnockback(velocity))
        velocity = player.hasSimulatedVelocity() ? player.getSimulatedVelocity() : player.getMotion();
    bool onGround = player.isOnGround();
    int32_t jumpDelay = player.getJumpDelay();
    const bool jumpHeld = packet.hasInputFlag((int32_t) PlayerAuthInputData::Jumping)
                          || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartJumping);
    const bool startJumping = packet.hasInputFlag((int32_t) PlayerAuthInputData::StartJumping);
    if (!jumpHeld)
        jumpDelay = 0;

    const PlayerPose pose = resolvePose(level, start, packet, player.isSimulatedSneaking(),
                                        player.isSimulatedCrawling());
    player.setSimulatedPose(pose.mSneaking, pose.mCrawling);
    const bool sneaking = pose.mSneaking;
    const bool crawling = pose.mCrawling;
    const bool pressingSneak = packet.hasInputFlag((int32_t) PlayerAuthInputData::Sneaking);

    float maxImpulse = 1.0f;
    if (player.getFlags().get(ActorFlag::UsingItem))
        maxImpulse *= CONSUMING_INPUT;
    const int32_t slowdownTicks = sneaking || crawling ? player.getSlowdownTicks() + 1 : 0;
    player.setSlowdownTicks(slowdownTicks);
    if (!flying && (sneaking || crawling)) {
        float sneakMultiplier = SNEAK_INPUT;
        if (slowdownTicks > SWIFT_SNEAK_DELAY_TICKS) {
            const ItemStack &leggings = player.getInventory().getArmor(PlayerInventory::ARMOR_LEGS);
            sneakMultiplier += SWIFT_SNEAK_PER_LEVEL
                               * (float) ItemEnchantments::getLevel(leggings, EnchantmentIds::SWIFT_SNEAK);
        }
        maxImpulse *= std::clamp(sneakMultiplier, 0.0f, 1.0f);
    }
    const float sideways = std::clamp(packet.mMotionX, -maxImpulse, maxImpulse) * IMPULSE_SCALE;
    const float forward = std::clamp(packet.mMotionY, -maxImpulse, maxImpulse) * IMPULSE_SCALE;

    float friction = AIR_FRICTION;
    float acceleration = sprinting ? SPRINT_AIR_SPEED : WALK_AIR_SPEED;
    if (flying) {
        // Same fly speed as the ability advertised by LoginHandler; BDS doubles it when sprinting.
        acceleration = 0.05f * (sprinting ? 2.0f : 1.0f);
        const bool ascend = input(packet, PlayerAuthInputData::Ascend);
        const bool descend = input(packet, PlayerAuthInputData::Descend);
        if (packet.mInputMode == PlayerInputMode::Mouse
            && std::max(std::fabs(packet.mMotionX), std::fabs(packet.mMotionY)) < 0.01f
            && !ascend && !descend && !jumpHeld && !pressingSneak)
            velocity.y *= 0.375f;
        if (ascend && descend) {
            velocity.y = 0.0f;
        } else {
            velocity.y += (ascend ? 0.15f : 0.0f) + (jumpHeld ? 0.05f : 0.0f)
                          - (descend ? 0.22f : 0.0f) - (pressingSneak ? 0.15f : 0.0f);
        }
    } else if (waterTravel) {
        const float depth = (float) std::clamp(ItemEnchantments::getLevel(boots, EnchantmentIds::DEPTH_STRIDER), 0, 3)
                            * (onGround ? 1.0f : 0.5f);
        const float movement = std::max(0.0f, player.getAttributes().get("minecraft:movement"))
                               * (sprinting ? SPRINT_SPEED_MULTIPLIER : 1.0f);
        acceleration = std::max(0.0f, player.getAttributes().get("minecraft:underwater_movement"));
        acceleration += (movement - acceleration) * depth / 3.0f;
        friction = sprinting ? 0.9f : 0.8f;
        friction += depth / 3.0f * (0.54600006f - friction);
    } else if (lavaTravel) {
        acceleration = std::max(0.0f, player.getAttributes().get("minecraft:lava_movement"));
        friction = 0.5f;
    } else if (onGround && !gliding) {
        const Block *ground = blockAt(level, start.x, start.y - GROUND_PROBE_DEPTH, start.z);
        const float groundFriction = blockFriction(ground);
        friction *= groundFriction;
        const float speed = std::max(0.0f, player.getAttributes().get("minecraft:movement"));
        float accelerationMultiplier = ground == nullptr ? 1.0f : ground->getAccelerationFrictionMultiplier();
        if (ItemEnchantments::getLevel(boots, EnchantmentIds::SOUL_SPEED) > 0)
            accelerationMultiplier = 1.0f;
        // Ground acceleration scales with the cube of (default friction / block friction), so
        // slippery blocks such as ice accelerate slowly but keep their momentum.
        const float accelerationFriction = (groundFriction * accelerationMultiplier) * AIR_FRICTION;
        const float baseFriction = AIR_FRICTION * DEFAULT_BLOCK_FRICTION;
        const float ratio = baseFriction / accelerationFriction;
        const float movement = sprinting ? speed * SPRINT_SPEED_MULTIPLIER : speed;
        acceleration = ((ratio * ratio) * ratio) * movement;
    }

    if (gliding && !waterTravel && !lavaTravel) {
        glide(velocity, packet, player.hasEffect(MobEffectId::SlowFalling));
    } else {
        moveRelative(velocity, sideways, forward, acceleration, packet.mRotation.y);
    }

    if (waterTravel || lavaTravel) {
        if (jumpHeld)
            velocity.y += 0.04f;
        if (pressingSneak || input(packet, PlayerAuthInputData::Descend))
            velocity.y -= 0.04f;
        if (waterTravel) {
            velocity.x += liquid.flow.x * 0.014f;
            velocity.z += liquid.flow.z * 0.014f;
            if (liquid.bubble) {
                velocity.y = liquid.dragDown ? std::max(-0.3f, velocity.y - 0.03f)
                                            : std::min(0.7f, velocity.y + 0.06f);
            }
        }
    }

    if (!flying && !gliding && !waterTravel && !lavaTravel && startJumping && onGround && jumpDelay <= 0) {
        const MobEffectInstance *jumpBoost = player.getEffects().get(MobEffectId::JumpBoost);
        const float jumpBonus = jumpBoost == nullptr ? 0.0f : JUMP_BOOST_PER_LEVEL * (float) jumpBoost->level();
        const float jumpHeight = JUMP_VELOCITY + jumpBonus;
        velocity.y = std::max(jumpHeight * jumpFactor(level, start), velocity.y);
        jumpDelay = JUMP_DELAY_TICKS;
        if (sprinting) {
            const float direction = packet.mRotation.y * 0.017453292f;
            velocity.x -= mojangSin(direction) * 0.2f;
            velocity.z += mojangCos(direction) * 0.2f;
        }
    }

    const Block *inside = blockAt(level, start.x, start.y, start.z);
    BlockTraversal traversal = inside == nullptr ? BlockTraversal::None : inside->getTraversal();
    if (traversal == BlockTraversal::None && player.hasSupportingBlock()) {
        const Vector3i &support = player.getSupportingBlock();
        const Block *supporting = blockAt(level, (float) support.x, (float) support.y, (float) support.z);
        if (supporting != nullptr && supporting->getTraversal() == BlockTraversal::Scaffolding)
            traversal = BlockTraversal::Scaffolding;
    }
    bool scaffoldDescend = false;
    if (flying || gliding || waterTravel || lavaTravel)
        traversal = BlockTraversal::None;
    if (traversal == BlockTraversal::Scaffolding) {
        if (pressingSneak) {
            velocity.y = -TRAVERSAL_SPEED;
            scaffoldDescend = true;
        } else if (jumpHeld) {
            velocity.y = TRAVERSAL_SPEED;
        }
    } else if (traversal == BlockTraversal::PowderSnow) {
        if (pressingSneak)
            velocity.y = -TRAVERSAL_SPEED;
        else if (jumpHeld && !boots.isAir() && boots.mDefinition->getIdentifier() == LEATHER_BOOTS)
            velocity.y = POWDER_SNOW_ASCEND_SPEED;
    }

    if (!flying && !gliding && inside != nullptr && inside->isClimbable() && traversal == BlockTraversal::None) {
        velocity.y = std::max(velocity.y, -CLIMB_SPEED);
        if (jumpHeld || player.hasHorizontalCollision())
            velocity.y = CLIMB_SPEED;
        if (sneaking && velocity.y < 0.0f)
            velocity.y = 0.0f;
    }

    AxisAlignedBB box = boxAt(start, pose.mHeight);
    Vector3f stuck;
    const bool isStuck = !spectator && stuckMultiplier(level, player, box, stuck);
    if (isStuck) {
        velocity.x *= stuck.x;
        velocity.y *= stuck.y;
        velocity.z *= stuck.z;
    }

    if (!flying && sneaking && !crawling && onGround && velocity.y <= 0.0f)
        avoidEdge(level, box, velocity);

    ActorMoveResult moved;
    if (spectator) {
        box = box.offset(velocity.x, velocity.y, velocity.z);
        moved.mMoved = velocity;
    } else {
        moved = ActorCollisionSystem::move(level, box, velocity, STEP_HEIGHT, onGround);
    }
    Vector3f simulated(box.mMinX + PLAYER_WIDTH * 0.5f, box.mMinY, box.mMinZ + PLAYER_WIDTH * 0.5f);
    const bool wasOnGround = onGround;
    onGround = moved.mOnGround;

    updateSupportingBlock(level, player, box, simulated, moved.mOnGround, velocity);
    const BlockBehavior &under = behaviorUnder(level, simulated, player);
    if (simulated.y == start.y && onGround && !sneaking && !pressingSneak && under.slowsWalking()) {
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
        velocity.y = wasOnGround || velocity.y >= 0.0f || pressingSneak ? 0.0f : landingVelocity(under, velocity.y);

    if (flying) {
        velocity.y *= 0.6f;
    } else if (waterTravel || lavaTravel) {
        velocity.y *= waterTravel ? 0.8f : 0.5f;
        const MobEffectInstance *levitation = player.getEffects().get(MobEffectId::Levitation);
        if (levitation != nullptr)
            velocity.y = velocity.y * 0.8f + 0.01f * (float) levitation->level();
        if (!waterTravel || !swimming)
            velocity.y -= waterTravel ? 0.005f : 0.02f;
    } else if (!gliding && !scaffoldDescend) {
        const MobEffectInstance *levitation = player.getEffects().get(MobEffectId::Levitation);
        if (levitation != nullptr) {
            velocity.y = velocity.y * 0.8f + 0.01f * (float) levitation->level();
        } else {
            const float gravity = player.hasEffect(MobEffectId::SlowFalling) && velocity.y < 0.0f
                                  ? SLOW_FALLING_GRAVITY : GRAVITY;
            velocity.y = (velocity.y - gravity) * GRAVITY_MULTIPLIER;
        }
    }
    if (!gliding || waterTravel || lavaTravel) {
        velocity.x *= friction;
        velocity.z *= friction;
    }
    if (!spectator)
        slideAlongHoney(level, box, velocity);
    if (gameType == (int32_t) GameType::Creative || spectator)
        player.setOnGround(onGround);

    if (jumpDelay > 0)
        jumpDelay -= 1;
    player.setJumpDelay(jumpDelay);

    const bool needsCorrection = player.getMovementValidator().reconcile(
            simulated, feetPosition, (uint64_t) packet.mTick,
            owner.getProperties().getPlayerPositionAcceptanceThreshold(), strict);
    player.setSimulatedPosition(simulated);
    player.setSimulatedVelocity(velocity);
    feetPosition = simulated;
    player.setMotion(velocity);

    if (!needsCorrection)
        return;

    // The client rewinds to the tick named in the correction and replays its inputs from the
    // server's position and velocity; the position is sent at eye level like other player
    // positions.
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
