#include "World/Actor/Movement/Systems/PlayerPredictionSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "Core/Debug/BedrockLog.h"

using namespace MovementPhysics;

void PlayerPredictionSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &prediction = context.prediction;

    const Vector3f reported = prediction.feetPosition;
    MovementCorrectionDetails details;
    Vector3f acceptedPosition = state.simulated;
    const bool needsCorrection = prediction.validator.reconcile(
            acceptedPosition, prediction.feetPosition, (uint64_t) input.packet.mTick,
            context.owner.getProperties().getPlayerPositionAcceptanceThreshold(), prediction.strict, &details);
    // BDS ReplayState (0x140E70E40) steers the next tick's velocity toward acceptPos;
    // it leaves this tick's physical position unchanged, including fine-threshold adoption.
    state.velocity.x += acceptedPosition.x - state.simulated.x;
    state.velocity.y += acceptedPosition.y - state.simulated.y;
    state.velocity.z += acceptedPosition.z - state.simulated.z;
    context.player.setSimulatedPosition(state.simulated);
    context.player.setSimulatedOnGround(state.onGround);
    context.player.setSimulatedVelocity(state.velocity);
    prediction.feetPosition = state.simulated;
    context.player.setMotion(state.velocity);

    if (!needsCorrection)
        return;

    uint64_t flagsLow = 0;
    uint64_t flagsHigh = 0;
    for (int32_t flag: input.packet.mInputData) {
        if (flag >= 0 && flag < 64)
            flagsLow |= uint64_t(1) << flag;
        else if (flag >= 64 && flag < 128)
            flagsHigh |= uint64_t(1) << (flag - 64);
    }
    const auto &travel = context.travel;
    const auto &contact = context.contact;
    const auto &abilities = context.abilities;
    const auto &timeline = context.player.getMovementInputs();
    // Keep the measured mismatch before position adoption overwrites the reported position.
    LOG_WARN(LogAreaID::Network,
             "[MovementCorrection] player=%s reason=%s tick=%llu server_tick=%lld previous_correction=%llu "
             "strict=%d error=(%.9g,%.9g,%.9g) distance_squared=%.9g threshold=%.9g fine_threshold=%.9g adjustments=%u "
             "start=(%.9g,%.9g,%.9g) reported=(%.9g,%.9g,%.9g) predicted=(%.9g,%.9g,%.9g) "
             "velocity=(%.9g,%.9g,%.9g) client_delta=(%.9g,%.9g,%.9g) "
             "input=(%.9g,%.9g) yaw=%.9g pitch=%.9g input_mode=%d flags=%016llx:%016llx "
             "ground=%d water=%d lava=%d feet_water=%d eyes_water=%d swim_probe=%d "
             "fly=%d sprint=%d swim=%d glide=%d jump=%d sneak=%d pose_height=%.9g "
             "collision=(%d,%d,%d) requested=(%.9g,%.9g,%.9g) moved=(%.9g,%.9g,%.9g) "
             "acceleration=%.9g friction=%.9g friction_modifier=%.9g pending_inputs=%zu credits=%llu "
             "accepted=(%.9g,%.9g,%.9g) acceptance_budget=%.9g acceptance_cooldown=%u",
             context.player.getName().c_str(), details.reason, (unsigned long long) input.packet.mTick,
             (long long) context.owner.getCurrentTick(), (unsigned long long) details.previousCorrectionTick,
             prediction.strict, details.error.x, details.error.y, details.error.z,
             details.distanceSquared, details.threshold, details.fineThreshold, (unsigned int) details.adjustments,
             state.start.x, state.start.y, state.start.z, reported.x, reported.y, reported.z,
             state.simulated.x, state.simulated.y, state.simulated.z,
             state.velocity.x, state.velocity.y, state.velocity.z,
             input.packet.mDelta.x, input.packet.mDelta.y, input.packet.mDelta.z,
             input.packet.mMotionX, input.packet.mMotionY, input.packet.mRotation.y, input.packet.mRotation.x,
             (int) input.packet.mInputMode, (unsigned long long) flagsHigh, (unsigned long long) flagsLow,
             state.onGround, contact.waterTravel, contact.lavaTravel, contact.liquid.feetInWater,
             contact.liquid.eyeInWater, contact.liquid.swimProbeInWater,
             abilities.flying, abilities.sprinting, abilities.swimming, abilities.gliding,
             input.jumpHeld, input.pressingSneak, travel.pose.mHeight,
             travel.moved.mCollidedX, travel.moved.mCollidedY, travel.moved.mCollidedZ,
             travel.moved.mRequested.x, travel.moved.mRequested.y, travel.moved.mRequested.z,
             travel.moved.mMoved.x, travel.moved.mMoved.y, travel.moved.mMoved.z,
             travel.acceleration, travel.friction, travel.frictionModifier,
             timeline.pendingCount(), (unsigned long long) timeline.credits(),
             travel.accepted.x, travel.accepted.y, travel.accepted.z,
             prediction.validator.clientAcceptance().mBudget,
             (unsigned int) prediction.validator.clientAcceptance().mCooldown);

    // The client rewinds to the tick named in the correction and replays its inputs from the
    // server's position and velocity; the position is sent at eye level like other player
    // positions.
    CorrectPlayerMovePredictionPacket correction;
    correction.mPredictionType = PredictionType::Player;
    correction.mPosition = Vector3f(state.simulated.x, state.simulated.y + PlayerMovementSimulator::PLAYER_BASE_OFFSET, state.simulated.z);
    correction.mDelta = state.velocity;
    correction.mVehicleRotation = Vector2f(input.packet.mRotation.x, input.packet.mRotation.y);
    correction.mHasVehicleAngularVelocity = false;
    correction.mOnGround = state.onGround;
    correction.mTick = (uint64_t) input.packet.mTick;
    context.owner.getNetworkHandler().send(context.id, correction, context.owner.getCodecContext());
}
