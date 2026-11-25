#include "World/Item/SpawnEggItem.h"

#include <iostream>
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
}

int main() {
    try {
        eggOfActor();
        std::cout << "spawn egg pick checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
    return 0;
}
