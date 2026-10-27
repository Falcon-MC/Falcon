#include "World/Actor/Movement/PlayerMovementPipeline.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

#include "World/Actor/Movement/Systems/BubbleColumnMovementSystem.h"
#include "World/Actor/Movement/Systems/ClimbOutOfLiquidSystem.h"
#include "World/Actor/Movement/Systems/CurrentSwimAmountSystem.h"
#include "World/Actor/Movement/Systems/FlyingPlayerStuckOnGroundWorkaroundSystem.h"
#include "World/Actor/Movement/Systems/GlideMoveSystem.h"
#include "World/Actor/Movement/Systems/GroundTravelSystem.h"
#include "World/Actor/Movement/Systems/LavaDragSystem.h"
#include "World/Actor/Movement/Systems/LavaTravelSystem.h"
#include "World/Actor/Movement/Systems/MovementAbilitiesSystem.h"
#include "World/Actor/Movement/Systems/MovementInputSystem.h"
#include "World/Actor/Movement/Systems/PlayerFlightDragSystem.h"
#include "World/Actor/Movement/Systems/PlayerFlightSystem.h"
#include "World/Actor/Movement/Systems/PlayerFrictionSystem.h"
#include "World/Actor/Movement/Systems/PlayerGravitySystem.h"
#include "World/Actor/Movement/Systems/PlayerGroundJumpSystem.h"
#include "World/Actor/Movement/Systems/PlayerLiquidContactSystem.h"
#include "World/Actor/Movement/Systems/PlayerLiquidCurrentSystem.h"
#include "World/Actor/Movement/Systems/PlayerLiquidJumpSystem.h"
#include "World/Actor/Movement/Systems/PlayerMoveRelativeSystem.h"
#include "World/Actor/Movement/Systems/PlayerMovementAdmissionSystem.h"
#include "World/Actor/Movement/Systems/PlayerMovementInitializationSystem.h"
#include "World/Actor/Movement/Systems/PlayerPostTravelSystem.h"
#include "World/Actor/Movement/Systems/PlayerPredictionSystem.h"
#include "World/Actor/Movement/Systems/PlayerTraversalSystem.h"
#include "World/Actor/Movement/Systems/PlayerTravelPreparationSystem.h"
#include "World/Actor/Movement/Systems/SwimControlSystem.h"
#include "World/Actor/Movement/Systems/UpdateHorizontalPoseSystem.h"
#include "World/Actor/Movement/Systems/WaterDragSystem.h"
#include "World/Actor/Movement/Systems/WaterSinkInputSystem.h"
#include "World/Actor/Movement/Systems/WaterTravelSystem.h"
#include "World/Actor/Movement/Systems/PlayerCollisionSystem.h"

#include <stdexcept>

namespace {
template <typename System>
bool execute(PlayerMovementContext &context) {
    System::tick(context);
    return true;
}

bool admit(PlayerMovementContext &context) {
    return PlayerMovementAdmissionSystem::prepare(context);
}

PlayerMovementSystemRegistry makePlayerMovementSchedule() {
    using Phase = PlayerMovementSystemRegistry::Phase;
    PlayerMovementSystemRegistry schedule;
    schedule.registerSystem(Phase::Admission, "PlayerMovementAdmissionSystem", admit);

    schedule.registerSystem(Phase::PreTravel, "MovementAbilitiesSystem", execute<MovementAbilitiesSystem>);
    schedule.registerSystem(Phase::PreTravel, "PlayerMovementInitializationSystem", execute<PlayerMovementInitializationSystem>);
    schedule.registerSystem(Phase::PreTravel, "UpdateHorizontalPoseSystem", execute<UpdateHorizontalPoseSystem>);
    schedule.registerSystem(Phase::PreTravel, "PlayerLiquidContactSystem", execute<PlayerLiquidContactSystem>);
    schedule.registerSystem(Phase::PreTravel, "MovementInputSystem", execute<MovementInputSystem>);
    schedule.registerSystem(Phase::PreTravel, "PlayerTravelPreparationSystem", execute<PlayerTravelPreparationSystem>);

    schedule.registerSystem(Phase::Travel, "PlayerFlightSystem", execute<PlayerFlightSystem>);
    schedule.registerSystem(Phase::Travel, "WaterTravelSystem", execute<WaterTravelSystem>);
    schedule.registerSystem(Phase::Travel, "LavaTravelSystem", execute<LavaTravelSystem>);
    schedule.registerSystem(Phase::Travel, "GroundTravelSystem", execute<GroundTravelSystem>);
    schedule.registerSystem(Phase::Travel, "PlayerLiquidCurrentSystem", execute<PlayerLiquidCurrentSystem>);
    schedule.registerSystem(Phase::Travel, "WaterSinkInputSystem", execute<WaterSinkInputSystem>);
    schedule.registerSystem(Phase::Travel, "PlayerLiquidJumpSystem", execute<PlayerLiquidJumpSystem>);
    schedule.registerSystem(Phase::Travel, "PlayerGroundJumpSystem", execute<PlayerGroundJumpSystem>);
    schedule.registerSystem(Phase::Travel, "SwimControlSystem", execute<SwimControlSystem>);
    schedule.registerSystem(Phase::Travel, "GlideMoveSystem", execute<GlideMoveSystem>);
    schedule.registerSystem(Phase::Travel, "PlayerMoveRelativeSystem", execute<PlayerMoveRelativeSystem>);
    schedule.registerSystem(Phase::Travel, "BubbleColumnMovementSystem", execute<BubbleColumnMovementSystem>);
    schedule.registerSystem(Phase::Travel, "PlayerTraversalSystem", execute<PlayerTraversalSystem>);
    schedule.registerSystem(Phase::Travel, "FlyingPlayerStuckOnGroundWorkaroundSystem",
                            execute<FlyingPlayerStuckOnGroundWorkaroundSystem>);
    schedule.registerSystem(Phase::Travel, "PlayerCollisionSystem", execute<PlayerCollisionSystem>);

    schedule.registerSystem(Phase::PostTravel, "PlayerFlightDragSystem", execute<PlayerFlightDragSystem>);
    schedule.registerSystem(Phase::PostTravel, "WaterDragSystem", execute<WaterDragSystem>);
    schedule.registerSystem(Phase::PostTravel, "LavaDragSystem", execute<LavaDragSystem>);
    schedule.registerSystem(Phase::PostTravel, "PlayerGravitySystem", execute<PlayerGravitySystem>);
    schedule.registerSystem(Phase::PostTravel, "PlayerFrictionSystem", execute<PlayerFrictionSystem>);
    schedule.registerSystem(Phase::PostTravel, "ClimbOutOfLiquidSystem", execute<ClimbOutOfLiquidSystem>);
    schedule.registerSystem(Phase::PostTravel, "PlayerPostTravelSystem", execute<PlayerPostTravelSystem>);

    schedule.registerSystem(Phase::Prediction, "PlayerPredictionSystem", execute<PlayerPredictionSystem>);
    schedule.registerSystem(Phase::Prediction, "CurrentSwimAmountSystem", execute<CurrentSwimAmountSystem>);
    return schedule;
}
} // namespace

void PlayerMovementSystemRegistry::registerSystem(Phase phase, std::string_view name, System system) {
    if (system == nullptr)
        throw std::invalid_argument("cannot register a null player movement system");
    mSystems.push_back({phase, name, system});
}

void PlayerMovementSystemRegistry::run(PlayerMovementContext &context) const {
    for (uint8_t phase = static_cast<uint8_t>(Phase::Admission);
         phase <= static_cast<uint8_t>(Phase::Prediction); ++phase) {
        for (const Entry &entry : mSystems) {
            if (static_cast<uint8_t>(entry.phase) != phase)
                continue;
            if (!entry.system(context))
                return;
        }
    }
}

void PlayerMovementPipeline::tick(PlayerMovementContext &context) {
    static const PlayerMovementSystemRegistry schedule = makePlayerMovementSchedule();
    schedule.run(context);
}
