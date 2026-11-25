#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Misc/CushionActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Vehicle/BoatActor.h"
#include "World/Item/SpawnEggItem.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void eggOfActor() {
        require(SpawnEggItem::eggOf("minecraft:pig") == "minecraft:pig_spawn_egg", "pig egg");
        require(SpawnEggItem::eggOf("minecraft:villager_v2") == "minecraft:villager_spawn_egg", "villager egg");
        require(SpawnEggItem::eggOf("minecraft:zombie_villager_v2") == "minecraft:zombie_villager_spawn_egg",
                "zombie villager egg");
        require(SpawnEggItem::eggOf("minecraft:evocation_illager") == "minecraft:evoker_spawn_egg", "evoker egg");
        require(SpawnEggItem::eggOf("minecraft:tropicalfish") == "minecraft:tropical_fish_spawn_egg",
                "tropical fish egg");

        const char *const actors[] = {"minecraft:pig", "minecraft:villager_v2", "minecraft:zombie_villager_v2",
                                      "minecraft:evocation_illager", "minecraft:tropicalfish"};
        for (const char *actor: actors)
            require(SpawnEggItem::spawnedActorOf(SpawnEggItem::eggOf(actor)) == actor, "egg spawns another actor");
    }

    std::string pickOf(const char *identifier) {
        std::unique_ptr<ServerActor> actor = ActorClassRegistry::create(100, identifier);
        require(actor != nullptr, "actor class missing");
        return actor->getPickItem();
    }

    void actorPickItems() {
        require(pickOf("minecraft:armor_stand") == "minecraft:armor_stand", "armor stand pick");
        require(pickOf("minecraft:painting") == "minecraft:painting", "painting pick");
        require(pickOf("minecraft:ender_crystal") == "minecraft:end_crystal", "end crystal pick");
        const char *const minecarts[] = {"minecraft:minecart", "minecraft:chest_minecart", "minecraft:hopper_minecart",
                                         "minecraft:tnt_minecart", "minecraft:command_block_minecart"};
        for (const char *minecart: minecarts)
            require(pickOf(minecart) == minecart, "minecart pick");

        std::unique_ptr<ServerActor> boat = ActorClassRegistry::create(100, BoatActor::BOAT);
        auto *wooden = dynamic_cast<BoatActor *>(boat.get());
        require(wooden != nullptr, "boat class missing");
        wooden->setVariant(BoatActor::variantOfItem("minecraft:cherry_boat"));
        require(wooden->getPickItem() == "minecraft:cherry_boat", "boat pick");
        std::unique_ptr<ServerActor> chestBoat = ActorClassRegistry::create(100, BoatActor::CHEST_BOAT);
        auto *raft = dynamic_cast<BoatActor *>(chestBoat.get());
        require(raft != nullptr, "chest boat class missing");
        raft->setVariant(BoatActor::RAFT_VARIANT);
        require(raft->getPickItem() == "minecraft:bamboo_chest_raft", "chest raft pick");

        std::unique_ptr<ServerActor> cushion = ActorClassRegistry::create(100, CushionActor::IDENTIFIER);
        require(cushion->getPickItem() == "minecraft:white_cushion", "cushion pick");
    }
}

int main() {
    try {
        eggOfActor();
        actorPickItems();
        std::cout << "spawn egg pick checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
    return 0;
}
