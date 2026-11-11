#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/SizedActor.h"
#include "World/Actor/Vehicle/ChestVehicleActor.h"

FALCON_REGISTER_SIZED_ACTOR(Boat, "minecraft:boat", 1.3f, 0.5f, false);
FALCON_REGISTER_SIZED_ACTOR(Minecart, "minecraft:minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(HopperMinecart, "minecraft:hopper_minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(TntMinecart, "minecraft:tnt_minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(CommandBlockMinecart, "minecraft:command_block_minecart", 0.98f, 0.7f, false);

static const ActorClassRegistry::Registration gActorRegistrationChestBoat(
        "minecraft:chest_boat", [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<ChestVehicleActor>(runtimeId, identifier, ActorSize{1.3f, 0.5f});
        });

static const ActorClassRegistry::Registration gActorRegistrationChestMinecart(
        "minecraft:chest_minecart",
        [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<ChestVehicleActor>(runtimeId, identifier, ActorSize{0.98f, 0.7f});
        });
