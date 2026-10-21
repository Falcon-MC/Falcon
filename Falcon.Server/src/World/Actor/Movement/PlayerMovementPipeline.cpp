#include "World/Actor/Movement/PlayerMovementPipeline.h"
#include "World/Actor/Movement/Systems/ClimbOutOfLiquidSystem.h"
#include "World/Actor/Movement/Systems/FlyingPlayerStuckOnGroundWorkaroundSystem.h"
#include "World/Actor/Movement/Systems/PlayerMovementAdmissionSystem.h"
#include "World/Actor/Movement/Systems/MovementAbilitiesSystem.h"
#include "World/Actor/Movement/Systems/CurrentSwimAmountSystem.h"

#include "World/Actor/Movement/Systems/PlayerMovementInitializationSystem.h"
#include "World/Actor/Movement/Systems/UpdateHorizontalPoseSystem.h"
#include "World/Actor/Movement/Systems/PlayerLiquidContactSystem.h"
#include "World/Actor/Movement/Systems/MovementInputSystem.h"
#include "World/Actor/Movement/Systems/PlayerTravelPreparationSystem.h"
#include "World/Actor/Movement/Systems/PlayerFlightSystem.h"
#include "World/Actor/Movement/Systems/WaterTravelSystem.h"
#include "World/Actor/Movement/Systems/LavaTravelSystem.h"
#include "World/Actor/Movement/Systems/GroundTravelSystem.h"
#include "World/Actor/Movement/Systems/PlayerLiquidCurrentSystem.h"
#include "World/Actor/Movement/Systems/WaterSinkInputSystem.h"
#include "World/Actor/Movement/Systems/PlayerLiquidJumpSystem.h"
#include "World/Actor/Movement/Systems/SwimControlSystem.h"
#include "World/Actor/Movement/Systems/GlideMoveSystem.h"
#include "World/Actor/Movement/Systems/PlayerMoveRelativeSystem.h"
#include "World/Actor/Movement/Systems/BubbleColumnMovementSystem.h"
#include "World/Actor/Movement/Systems/PlayerGroundJumpSystem.h"
#include "World/Actor/Movement/Systems/PlayerTraversalSystem.h"
#include "World/Actor/Movement/Systems/PlayerCollisionSystem.h"
#include "World/Actor/Movement/Systems/PlayerFlightDragSystem.h"
#include "World/Actor/Movement/Systems/WaterDragSystem.h"
#include "World/Actor/Movement/Systems/LavaDragSystem.h"
#include "World/Actor/Movement/Systems/PlayerGravitySystem.h"
#include "World/Actor/Movement/Systems/PlayerFrictionSystem.h"
#include "World/Actor/Movement/Systems/PlayerPostTravelSystem.h"
#include "World/Actor/Movement/Systems/PlayerPredictionSystem.h"

void PlayerMovementPipeline::tick(PlayerMovementContext &context) {
    // The input queue invokes this same order once for each normal or catch-up input.
    if (!PlayerMovementAdmissionSystem::prepare(context))
        return;
    MovementAbilitiesSystem::tick(context);
    PlayerMovementInitializationSystem::tick(context);
    UpdateHorizontalPoseSystem::tick(context);
    PlayerLiquidContactSystem::tick(context);
    MovementInputSystem::tick(context);
    PlayerTravelPreparationSystem::tick(context);
    PlayerFlightSystem::tick(context);
    WaterTravelSystem::tick(context);
    LavaTravelSystem::tick(context);
    GroundTravelSystem::tick(context);
    PlayerLiquidCurrentSystem::tick(context);
    WaterSinkInputSystem::tick(context);
    PlayerLiquidJumpSystem::tick(context);
    PlayerGroundJumpSystem::tick(context);
    SwimControlSystem::tick(context);
    GlideMoveSystem::tick(context);
    PlayerMoveRelativeSystem::tick(context);
    BubbleColumnMovementSystem::tick(context);
    PlayerTraversalSystem::tick(context);
    FlyingPlayerStuckOnGroundWorkaroundSystem::tick(context);
    PlayerCollisionSystem::tick(context);
    PlayerFlightDragSystem::tick(context);
    WaterDragSystem::tick(context);
    LavaDragSystem::tick(context);
    PlayerGravitySystem::tick(context);
    PlayerFrictionSystem::tick(context);
    ClimbOutOfLiquidSystem::tick(context);
    PlayerPostTravelSystem::tick(context);
    PlayerPredictionSystem::tick(context);
    CurrentSwimAmountSystem::tick(context);
}
