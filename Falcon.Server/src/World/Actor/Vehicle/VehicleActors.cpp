#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/SizedActor.h"
#include "World/Actor/Vehicle/ChestVehicleActor.h"

namespace {
    const ActorSize MINECART_SIZE{0.98f, 0.7f};

    class MinecartActor final : public SizedActor {
    public:
        MinecartActor(uint64_t runtimeId, const std::string &identifier)
                : SizedActor(runtimeId, identifier, MINECART_SIZE, false) {}

        bool isMinecart() const override {
            return true;
        }
    };
}

FALCON_REGISTER_SIZED_ACTOR(Boat, "minecraft:boat", 1.3f, 0.5f, false);
FALCON_REGISTER_ACTOR(MinecartActor, "minecraft:minecart");

static const ActorClassRegistry::Registration gActorRegistrationHopperMinecart(
        "minecraft:hopper_minecart",
        [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

static const ActorClassRegistry::Registration gActorRegistrationTntMinecart(
        "minecraft:tnt_minecart",
        [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

static const ActorClassRegistry::Registration gActorRegistrationCommandBlockMinecart(
        "minecraft:command_block_minecart",
        [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

static const ActorClassRegistry::Registration gActorRegistrationChestBoat(
        "minecraft:chest_boat", [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<ChestVehicleActor>(runtimeId, identifier, ActorSize{1.3f, 0.5f}, false);
        });

static const ActorClassRegistry::Registration gActorRegistrationChestMinecart(
        "minecraft:chest_minecart",
        [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<ChestVehicleActor>(runtimeId, identifier, MINECART_SIZE, true);
        });
