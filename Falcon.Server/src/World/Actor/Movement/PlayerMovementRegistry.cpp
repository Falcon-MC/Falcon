#include "World/Actor/Movement/PlayerMovementRegistry.h"

void PlayerMovementRegistry::prepare(Entity entity, const PlayerAuthInputPacket &packet, Vector3f &reportedPosition,
                                     PlayerMovementValidator &validator, bool sprinting) {
    auto reset = [entity](auto &pool) { pool.components.try_emplace(entity); };
    std::apply([&](auto &...pool) { (reset(pool), ...); }, mPools);

    get<StateVectorComponent>(entity) = {};
    MoveInputComponent &input = get<MoveInputComponent>(entity);
    input = {};
    input.packet = &packet;
    MovementAbilitiesComponent &abilities = get<MovementAbilitiesComponent>(entity);
    abilities = {};
    abilities.sprinting = sprinting;
    get<PlayerLiquidContactComponent>(entity) = {};
    PlayerPredictionComponent &prediction = get<PlayerPredictionComponent>(entity);
    prediction = {};
    prediction.feetPosition = &reportedPosition;
    prediction.validator = &validator;
    get<PlayerTravelComponent>(entity) = {};
    get<PlayerPoseComponent>(entity) = {};
    get<AutoClimbTravelFlagComponent>(entity) = {};
    get<WaterTravelFlagComponent>(entity) = {};
    get<LiquidTravelFlagComponent>(entity) = {};
}

void PlayerMovementRegistry::remove(Entity entity) {
    std::apply([entity](auto &...pool) { (pool.components.erase(entity), ...); }, mPools);
}
