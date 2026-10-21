#pragma once

#include "World/Actor/Movement/MovementPhysics.h"
#include "World/Actor/Movement/Components/StateVectorComponent.h"
#include "World/Actor/Movement/Components/MoveInputComponent.h"
#include "World/Actor/Movement/Components/MovementAbilitiesComponent.h"
#include "World/Actor/Movement/Components/PlayerLiquidContactComponent.h"
#include "World/Actor/Movement/Components/PlayerPredictionComponent.h"
#include "World/Actor/Movement/Components/PlayerTravelComponent.h"

class Level;
class NetworkIdentifier;
class ServerNetworkHandler;
class ServerPlayer;

/** One input's working components, shared by every stage and every replay path. */
struct PlayerMovementContext {
    ServerNetworkHandler &owner;
    const NetworkIdentifier &id;
    ServerPlayer &player;
    Level &level;
    StateVectorComponent state;
    MoveInputComponent input;
    MovementAbilitiesComponent abilities;
    PlayerLiquidContactComponent contact;
    PlayerPredictionComponent prediction;
    PlayerTravelComponent travel;
};
