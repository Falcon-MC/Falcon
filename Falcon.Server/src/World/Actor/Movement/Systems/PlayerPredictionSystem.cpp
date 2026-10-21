#include "World/Actor/Movement/Systems/PlayerPredictionSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"

using namespace MovementPhysics;

void PlayerPredictionSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &prediction = context.prediction;

    const bool needsCorrection = prediction.validator.reconcile(
            state.simulated, prediction.feetPosition, (uint64_t) input.packet.mTick,
            context.owner.getProperties().getPlayerPositionAcceptanceThreshold(), prediction.strict);
    context.player.setSimulatedPosition(state.simulated);
    context.player.setSimulatedVelocity(state.velocity);
    prediction.feetPosition = state.simulated;
    context.player.setMotion(state.velocity);

    if (!needsCorrection)
        return;

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
