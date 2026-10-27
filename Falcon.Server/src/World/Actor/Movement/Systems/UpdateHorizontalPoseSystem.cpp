#include "World/Actor/Movement/Systems/UpdateHorizontalPoseSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

namespace MovementPhysics {

    bool fits(Level &level, const Vector3f &feet, float height) {
        // BDS PlayerBoundingBoxStateUpdateSystem (0x140F97340) insets fit probes by 0.01.
        return !collides(level, boxAt(feet, height).expand(-0.01f, -0.01f, -0.01f));
    }

    PlayerPoseComponent resolvePose(Level &level, const Vector3f &feet, const PlayerAuthInputPacket &packet,
                           bool wasSneaking, bool wasCrawling) {
        const bool sneakDown = packet.hasInputFlag((int32_t) PlayerAuthInputData::SneakDown);
        const bool startSneaking = packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSneaking);
        const bool stopSneaking = packet.hasInputFlag((int32_t) PlayerAuthInputData::StopSneaking);
        const bool wantSneak = (sneakDown || startSneaking) && !stopSneaking;

        PlayerPoseComponent pose;
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
}

void UpdateHorizontalPoseSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    context.pose = resolvePose(context.level, state.start, input.getPacket(), context.player.isSimulatedSneaking(),
                                        context.player.isSimulatedCrawling());
    // BDS PlayerBoundingBoxStateUpdateSystem and SneakTriggerIntentSystem
    // (0x140F97340/0x140E77100) keep a horizontal pose when neither standing nor crouching fits.
    if (!abilities.flying && !abilities.gliding && !fits(context.level, state.start, PLAYER_HEIGHT)) {
        if (!fits(context.level, state.start, PLAYER_SNEAKING_HEIGHT)
            && fits(context.level, state.start, PLAYER_CRAWLING_HEIGHT)) {
            const bool previousHorizontal = context.player.getFlags().get(ActorFlag::Swimming)
                                            || context.player.getFlags().get(ActorFlag::Gliding)
                                            || context.player.isSimulatedCrawling();
            const float previousHeight = context.player.isSimulatedSneaking() ? PLAYER_SNEAKING_HEIGHT : PLAYER_HEIGHT;
            const LiquidContact previousContact = LiquidBlocksFetch::at(context.level, state.start,
                                                                        previousHorizontal, previousHeight);
            abilities.swimming = previousContact.water;
            context.pose.mCrawling = !abilities.swimming;
            context.pose.mSneaking = false;
        } else if (fits(context.level, state.start, PLAYER_SNEAKING_HEIGHT)) {
            const float movementSquared = input.getPacket().mMotionX * input.getPacket().mMotionX
                                          + input.getPacket().mMotionY * input.getPacket().mMotionY;
            if (context.pose.mCrawling || !abilities.swimming || movementSquared < 0.49999997f) {
                context.pose.mCrawling = false;
                context.pose.mSneaking = true;
                abilities.swimming = false;
            }
        }
    } else if (context.pose.mCrawling && fits(context.level, state.start, PLAYER_HEIGHT)) {
        context.pose.mCrawling = false;
    }
    context.pose.mHeight = context.pose.mCrawling ? PLAYER_CRAWLING_HEIGHT
                         : context.pose.mSneaking ? PLAYER_SNEAKING_HEIGHT : PLAYER_HEIGHT;
    context.player.setSimulatedSwimming(abilities.swimming);
    const bool horizontalPose = abilities.swimming || abilities.gliding || context.pose.mCrawling;
    if (horizontalPose)
        context.pose.mHeight = PLAYER_WIDTH;
    context.player.setSimulatedPose(context.pose.mSneaking, context.pose.mCrawling);
}
