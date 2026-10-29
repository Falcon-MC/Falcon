#pragma once

#include "World/Actor/Movement/Components/AutoClimbTravelFlagComponent.h"
#include "World/Actor/Movement/Components/LiquidTravelFlagComponent.h"
#include "World/Actor/Movement/Components/MoveInputComponent.h"
#include "World/Actor/Movement/Components/MovementAbilitiesComponent.h"
#include "World/Actor/Movement/Components/PlayerLiquidContactComponent.h"
#include "World/Actor/Movement/Components/PlayerPoseComponent.h"
#include "World/Actor/Movement/Components/PlayerPredictionComponent.h"
#include "World/Actor/Movement/Components/PlayerTravelComponent.h"
#include "World/Actor/Movement/Components/StateVectorComponent.h"
#include "World/Actor/Movement/Components/WaterTravelFlagComponent.h"

#include <cstdint>
#include <tuple>
#include <unordered_map>

/** Per-player ECS storage for the components consumed by the movement systems. */
class PlayerMovementRegistry {
public:
    using Entity = uint64_t;

    void prepare(Entity entity, const PlayerAuthInputPacket &packet, Vector3f &reportedPosition,
                 PlayerMovementValidator &validator, bool sprinting);
    void remove(Entity entity);

    template <typename Component>
    Component &get(Entity entity) {
        return std::get<Pool<Component>>(mPools).components.at(entity);
    }

private:
    template <typename Component>
    struct Pool {
        std::unordered_map<Entity, Component> components;
    };

    using Pools = std::tuple<Pool<StateVectorComponent>, Pool<MoveInputComponent>, Pool<MovementAbilitiesComponent>,
                             Pool<PlayerLiquidContactComponent>, Pool<PlayerPredictionComponent>,
                             Pool<PlayerTravelComponent>, Pool<PlayerPoseComponent>,
                             Pool<AutoClimbTravelFlagComponent>, Pool<WaterTravelFlagComponent>,
                             Pool<LiquidTravelFlagComponent>>;

    Pools mPools;
};
