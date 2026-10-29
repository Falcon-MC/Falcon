#pragma once

#include "World/Actor/Movement/MovementPhysics.h"
#include "World/Actor/Movement/PlayerMovementRegistry.h"

class Level;
class NetworkIdentifier;
class ServerNetworkHandler;
class ServerPlayer;

/** The current player entity and world services supplied to one system invocation. */
struct PlayerMovementContext {
    ServerNetworkHandler &owner;
    const NetworkIdentifier &id;
    ServerPlayer &player;
    Level &level;
    PlayerMovementRegistry &registry;
    PlayerMovementRegistry::Entity entity;
    StateVectorComponent &state;
    MoveInputComponent &input;
    MovementAbilitiesComponent &abilities;
    PlayerLiquidContactComponent &contact;
    PlayerPredictionComponent &prediction;
    PlayerTravelComponent &travel;
    PlayerPoseComponent &pose;
    AutoClimbTravelFlagComponent &autoClimb;
    WaterTravelFlagComponent &waterTravel;
    LiquidTravelFlagComponent &liquidTravel;

    PlayerMovementContext(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                          Level &level, PlayerMovementRegistry &registry, PlayerMovementRegistry::Entity entity)
            : owner(owner), id(id), player(player), level(level), registry(registry), entity(entity),
              state(registry.get<StateVectorComponent>(entity)), input(registry.get<MoveInputComponent>(entity)),
              abilities(registry.get<MovementAbilitiesComponent>(entity)),
              contact(registry.get<PlayerLiquidContactComponent>(entity)),
              prediction(registry.get<PlayerPredictionComponent>(entity)),
              travel(registry.get<PlayerTravelComponent>(entity)), pose(registry.get<PlayerPoseComponent>(entity)),
              autoClimb(registry.get<AutoClimbTravelFlagComponent>(entity)),
              waterTravel(registry.get<WaterTravelFlagComponent>(entity)),
              liquidTravel(registry.get<LiquidTravelFlagComponent>(entity)) {}
};
