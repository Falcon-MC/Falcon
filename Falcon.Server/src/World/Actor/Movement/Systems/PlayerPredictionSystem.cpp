#include "World/Actor/Movement/Systems/PlayerPredictionSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "Core/Debug/BedrockLog.h"

using namespace MovementPhysics;

namespace {
    // a client applies a correction on its next input; a few inputs absorb one already in flight
    const uint32_t MAX_IGNORED_CORRECTIONS = 3;
}

void PlayerPredictionSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &prediction = context.prediction;

    uint64_t flagsLow = 0;
    uint64_t flagsHigh = 0;
    for (int32_t flag: input.getPacket().mInputData) {
        if (flag >= 0 && flag < 64)
            flagsLow |= uint64_t(1) << flag;
        else if (flag >= 64 && flag < 128)
            flagsHigh |= uint64_t(1) << (flag - 64);
    }
    const auto &abilitiesBeforeCorrection = context.abilities;
    MovementTraceFrame frame;
    frame.tick = (uint64_t) input.getPacket().mTick;
    frame.flagsLow = flagsLow;
    frame.flagsHigh = flagsHigh;
    frame.start = state.start;
    frame.reported = *prediction.feetPosition;
    frame.predicted = state.simulated;
    frame.previousVelocity = context.player.hasSimulatedVelocity() ? context.player.getSimulatedVelocity()
                                                                   : context.player.getMotion();
    frame.physicalVelocity = state.velocity;
    frame.accepted = context.travel.accepted;
    frame.acceptanceBudget = prediction.validator->clientAcceptance().mBudget;
    frame.clientDelta = input.getPacket().mDelta;
    frame.rotation = input.getPacket().mRotation;
    frame.motionX = input.getPacket().mMotionX;
    frame.motionY = input.getPacket().mMotionY;
    frame.swimAmount = context.player.getSwimAmount().mCurrent;
    frame.movementSpeed = context.player.getAttributes().get("minecraft:movement");
    frame.state = (uint32_t) state.onGround | ((uint32_t) context.waterTravel.active << 1)
                  | ((uint32_t) context.liquidTravel.lava << 2) | ((uint32_t) abilitiesBeforeCorrection.flying << 3)
                  | ((uint32_t) abilitiesBeforeCorrection.sprinting << 4)
                  | ((uint32_t) abilitiesBeforeCorrection.swimming << 5) | ((uint32_t) input.jumpHeld << 6)
                  | ((uint32_t) context.contact.liquid.eyeSubmerged << 7);
    prediction.validator->trace().record(frame);

    // a simulated player reports what the server simulates: nothing to reconcile or correct
    if (context.player.isSimulated()) {
        context.player.setSimulatedPosition(state.simulated);
        context.player.setSimulatedOnGround(state.onGround);
        context.player.setSimulatedVelocity(state.velocity);
        *prediction.feetPosition = state.simulated;
        context.player.setMotion(state.velocity);
        return;
    }

    const Vector3f reported = *prediction.feetPosition;
    // the exit moves the player itself; reconciling it would turn up to 15 blocks into velocity
    if (prediction.validator->stillSeated(state.start, reported)) {
        state.simulated = reported;
        state.velocity = Vector3f(0.0f, 0.0f, 0.0f);
    } else if (prediction.validator->takeDismountPosition(state.simulated, reported)) {
        state.simulated = reported;
        // jumping off a seat: the client lifts off on the exit input itself (0.42, then 0.333, 0.248...); the
        // replay skipped that tick while riding, so it starts the fall with the jump's next-tick velocity
        if (input.jumpHeld) {
            const MobEffectInstance *jumpBoost = context.player.getEffects().get(MobEffectId::JumpBoost);
            const float jumpBonus = jumpBoost == nullptr ? 0.0f : JUMP_BOOST_PER_LEVEL * (float) jumpBoost->level();
            // a ground jump: only from a floor out of liquid; off a boat on water or a minecart in the air the
            // client swims or falls
            const Vector3f takeOff(reported.x, reported.y - (JUMP_VELOCITY + jumpBonus), reported.z);
            const LiquidContact liquid = LiquidBlocksFetch::at(context.level, takeOff);
            if (!liquid.water && !liquid.lava
                && MovementHandler::checkGroundState(context.level, takeOff, &context.player)) {
                state.velocity.y = (JUMP_VELOCITY + jumpBonus - GRAVITY) * GRAVITY_MULTIPLIER;
                state.jumpDelay = JUMP_DELAY_TICKS;
            }
        }
    }
    // measured on every input: the validator skips its comparison right after a correction and corrections are
    // spaced: most inputs of a dropped correction would look fine
    const float errorX = reported.x - state.simulated.x;
    const float errorY = reported.y - state.simulated.y;
    const float errorZ = reported.z - state.simulated.z;
    const bool diverging = errorX * errorX + errorY * errorY + errorZ * errorZ
                           > context.owner.getProperties().getPlayerPositionAcceptanceThreshold();
    MovementCorrectionDetails details;
    Vector3f acceptedPosition = state.simulated;
    const bool needsCorrection = prediction.validator->reconcile(
            acceptedPosition, *prediction.feetPosition, (uint64_t) input.getPacket().mTick,
            context.owner.getProperties().getPlayerPositionAcceptanceThreshold(), prediction.strict, &details);
    // ReplayState (0x140E70E40) steers next tick's velocity toward acceptPos; this tick's position stays,
    // fine-threshold adoption included
    state.velocity.x += acceptedPosition.x - state.simulated.x;
    state.velocity.y += acceptedPosition.y - state.simulated.y;
    state.velocity.z += acceptedPosition.z - state.simulated.z;
    context.player.setSimulatedPosition(state.simulated);
    context.player.setSimulatedOnGround(state.onGround);
    context.player.setSimulatedVelocity(state.velocity);
    *prediction.feetPosition = state.simulated;
    context.player.setMotion(state.velocity);

    // still diverging after acking a correction: the client dropped it (movement "disabler"); it can't be moved back
    if (prediction.validator->noteInputAfterCorrection(input.getPacket().mTick, diverging, reported)
        >= MAX_IGNORED_CORRECTIONS) {
        context.owner._disconnect(context.id, DisconnectReason::KickedForExploit,
                                  context.player.localize("falcon.disconnect.kicked"));
        return;
    }
    if (needsCorrection && diverging && prediction.validator->armCorrectionCheck(state.simulated, reported)) {
        ServerPlayer &player = context.player;
        player.getAcknowledgements().add([&player]() {
            player.getMovementValidator().acknowledgeCorrection(player.getMovementInputs().lastReceivedTick());
        });
    }

    if (!needsCorrection)
        return;

    const auto &travel = context.travel;
    const auto &contact = context.contact;
    const auto &abilities = context.abilities;
    const auto &timeline = context.player.getMovementInputs();
    // keep the measured mismatch before position adoption overwrites the reported position
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
             context.player.getName().c_str(), details.reason, (unsigned long long) input.getPacket().mTick,
             (long long) context.owner.getCurrentTick(), (unsigned long long) details.previousCorrectionTick,
             prediction.strict, details.error.x, details.error.y, details.error.z, details.distanceSquared,
             details.threshold, details.fineThreshold, (unsigned int) details.adjustments, state.start.x, state.start.y,
             state.start.z, reported.x, reported.y, reported.z, state.simulated.x, state.simulated.y, state.simulated.z,
             state.velocity.x, state.velocity.y, state.velocity.z, input.getPacket().mDelta.x,
             input.getPacket().mDelta.y, input.getPacket().mDelta.z, input.getPacket().mMotionX,
             input.getPacket().mMotionY, input.getPacket().mRotation.y, input.getPacket().mRotation.x,
             (int) input.getPacket().mInputMode, (unsigned long long) flagsHigh, (unsigned long long) flagsLow,
             state.onGround, context.waterTravel.active, context.liquidTravel.lava, contact.liquid.feetInWater,
             contact.liquid.eyeInWater, contact.liquid.swimProbeInWater, abilities.flying, abilities.sprinting,
             abilities.swimming, abilities.gliding, input.jumpHeld, input.pressingSneak, context.pose.mHeight,
             travel.moved.mCollidedX, travel.moved.mCollidedY, travel.moved.mCollidedZ, travel.moved.mRequested.x,
             travel.moved.mRequested.y, travel.moved.mRequested.z, travel.moved.mMoved.x, travel.moved.mMoved.y,
             travel.moved.mMoved.z, travel.acceleration, travel.friction, travel.frictionModifier,
             timeline.pendingCount(), (unsigned long long) timeline.credits(), travel.accepted.x, travel.accepted.y,
             travel.accepted.z, prediction.validator->clientAcceptance().mBudget,
             (unsigned int) prediction.validator->clientAcceptance().mCooldown);

    const auto &trace = prediction.validator->trace();
    for (size_t i = 0; i < trace.size(); ++i) {
        const auto &entry = trace.at(i);
        LOG_WARN(LogAreaID::Network,
                 "[MovementTrace] player=%s correction_tick=%llu tick=%llu state=%u flags=%016llx:%016llx "
                 "start=(%.9g,%.9g,%.9g) reported=(%.9g,%.9g,%.9g) predicted=(%.9g,%.9g,%.9g) "
                 "previous_velocity=(%.9g,%.9g,%.9g) physical_velocity=(%.9g,%.9g,%.9g) "
                 "client_delta=(%.9g,%.9g,%.9g) input=(%.9g,%.9g) yaw=%.9g pitch=%.9g "
                 "accepted=(%.9g,%.9g,%.9g) acceptance_budget=%.9g swim_amount=%.9g movement_speed=%.9g",
                 context.player.getName().c_str(), (unsigned long long) input.getPacket().mTick,
                 (unsigned long long) entry.tick, entry.state, (unsigned long long) entry.flagsHigh,
                 (unsigned long long) entry.flagsLow, entry.start.x, entry.start.y, entry.start.z, entry.reported.x,
                 entry.reported.y, entry.reported.z, entry.predicted.x, entry.predicted.y, entry.predicted.z,
                 entry.previousVelocity.x, entry.previousVelocity.y, entry.previousVelocity.z, entry.physicalVelocity.x,
                 entry.physicalVelocity.y, entry.physicalVelocity.z, entry.clientDelta.x, entry.clientDelta.y,
                 entry.clientDelta.z, entry.motionX, entry.motionY, entry.rotation.y, entry.rotation.x,
                 entry.accepted.x, entry.accepted.y, entry.accepted.z, entry.acceptanceBudget, entry.swimAmount,
                 entry.movementSpeed);
    }

    // the client rewinds to the correction's tick and replays from the server's position and velocity;
    // position at eye level like other player positions
    CorrectPlayerMovePredictionPacket correction;
    correction.mPredictionType = PredictionType::Player;
    correction.mPosition = Vector3f(state.simulated.x, state.simulated.y + PlayerMovementSimulator::PLAYER_BASE_OFFSET,
                                    state.simulated.z);
    correction.mDelta = state.velocity;
    correction.mVehicleRotation = Vector2f(input.getPacket().mRotation.x, input.getPacket().mRotation.y);
    correction.mHasVehicleAngularVelocity = false;
    correction.mOnGround = state.onGround;
    correction.mTick = (uint64_t) input.getPacket().mTick;
    context.owner.getNetworkHandler().send(context.id, correction, context.owner.getCodecContext());
}
