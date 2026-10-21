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
        return !collides(level, boxAt(feet, height));
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
    auto &travel = context.travel;

    travel.pose = resolvePose(context.level, state.start, input.packet, context.player.isSimulatedSneaking(),
                                        context.player.isSimulatedCrawling());
    const bool horizontalPose = abilities.swimming || abilities.gliding || travel.pose.mCrawling;
    if (horizontalPose)
        travel.pose.mHeight = PLAYER_WIDTH;
    context.player.setSimulatedPose(travel.pose.mSneaking, travel.pose.mCrawling);
    travel.sneaking = travel.pose.mSneaking;
    travel.crawling = travel.pose.mCrawling;
}
