#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t TICKS_PER_SECOND = 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Hoglins", "", 1};
        Level &mLevel;

        explicit Arena(DimensionType dimension) : mLevel(mOwner.getWorlds().getDefault().getLevel(dimension)) {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x <= 8; x++) {
                for (int32_t z = -8; z <= 8; z++)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void ticks(int32_t count) {
            // the world tick moves with the overworld, as a server tick does
            for (int32_t tick = 0; tick < count; tick++) {
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                // the server tick stays put without a full tick: a reservation for another tick frees the path budget
                PathFinder::tryReserveSearch(-1);
                mOwner.tickActors();
            }
        }

        size_t countOf(const char *identifier) {
            size_t found = 0;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    ++found;
            }
            return found;
        }
    };

    void zoglinConversion() {
        Arena arena(DimensionType::Overworld);
        MobActor &adult = arena.spawn("minecraft:hoglin", Vector3f(0.5f, 64.0f, 0.5f), "spawn_adult");
        MobActor &baby = arena.spawn("minecraft:hoglin", Vector3f(4.5f, 64.0f, 0.5f), "spawn_baby");
        arena.ticks(2);
        require(adult.getFlags().get(ActorFlag::Shaking) && baby.getFlags().get(ActorFlag::Shaking),
                "hoglins do not shake in the Overworld");
        arena.ticks(15 * TICKS_PER_SECOND);
        require(arena.countOf("minecraft:hoglin") == 0 && arena.countOf("minecraft:zoglin") == 2,
                "the hoglins did not become zoglins after 15 seconds");
        size_t babies = 0;
        for (auto &entry: arena.mOwner.getActors()) {
            MobActor *zoglin = dynamic_cast<MobActor *>(entry.second.get());
            if (zoglin == nullptr || zoglin->getTypeId() != "minecraft:zoglin")
                continue;
            require(zoglin->hasComponentGroup("zoglin_adult") != zoglin->hasComponentGroup("zoglin_baby"),
                    "a zoglin is neither adult nor baby");
            babies += zoglin->hasComponentGroup("zoglin_baby") ? 1 : 0;
        }
        require(babies == 1, "the baby hoglin did not become a baby zoglin");
    }

    void staysInTheNether() {
        Arena arena(DimensionType::Nether);
        MobActor &hoglin = arena.spawn("minecraft:hoglin", Vector3f(0.5f, 64.0f, 0.5f), "spawn_adult");
        arena.ticks(16 * TICKS_PER_SECOND);
        require(!hoglin.getFlags().get(ActorFlag::Shaking) && arena.countOf("minecraft:zoglin") == 0,
                "a hoglin converted in the Nether");
    }

    void flees(const char *block) {
        Arena arena(DimensionType::Nether);
        arena.player(1, Vector3f(-6.5f, 64.0f, -6.5f));
        arena.mLevel.setBlockState(0, 64, 0, BlockState(block));
        MobActor &hoglin = arena.spawn("minecraft:hoglin", Vector3f(2.5f, 64.0f, 0.5f), "spawn_adult");
        arena.ticks(3 * TICKS_PER_SECOND);
        const Vector3f position = hoglin.getPosition();
        require(position.x > 4.0f, std::string("the hoglin did not run from ") + block + ", x "
                                           + std::to_string(position.x));
    }

    void crimsonFungusLove() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &hoglin = arena.spawn("minecraft:hoglin", Vector3f(0.5f, 64.0f, 2.5f), "spawn_adult");
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:warped_fungus", 1));
        InventoryHandler::interactWithEntity(arena.mOwner, player, hoglin.getRuntimeId(), Vector3f());
        require(!hoglin.getFlags().get(ActorFlag::InLove), "a warped fungus made the hoglin fall in love");

        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:crimson_fungus", 1));
        InventoryHandler::interactWithEntity(arena.mOwner, player, hoglin.getRuntimeId(), Vector3f());
        require(hoglin.getFlags().get(ActorFlag::InLove), "a crimson fungus did not make the hoglin fall in love");
    }

    void zoglinTargets() {
        Arena arena(DimensionType::Nether);
        MobActor &zoglin = arena.spawn("minecraft:zoglin", Vector3f(0.5f, 64.0f, 0.5f), "minecraft:as_adult");
        MobActor &other = arena.spawn("minecraft:zoglin", Vector3f(2.5f, 64.0f, 0.5f), "minecraft:as_adult");
        MobActor &creeper = arena.spawn("minecraft:creeper", Vector3f(0.5f, 64.0f, 3.5f), "minecraft:entity_spawned");
        require(!zoglin.canTarget(zoglin), "a zoglin can target itself");
        const json::Value *goal = zoglin.getComponent("minecraft:behavior.nearest_attackable_target");
        const json::Value &filters = *goal->get("entity_types")->mArray[0]->get("filters");
        require(!EntityFilter::test(filters, arena.mOwner, zoglin, &other), "a zoglin targets zoglins");
        require(!EntityFilter::test(filters, arena.mOwner, zoglin, &creeper), "a zoglin targets creepers");
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(4.5f, 64.0f, 3.5f), "minecraft:entity_spawned");
        require(EntityFilter::test(filters, arena.mOwner, zoglin, &pig), "a zoglin ignores other mobs");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        zoglinConversion();
        staysInTheNether();
        crimsonFungusLove();
        zoglinTargets();
        flees("minecraft:warped_fungus");
        flees("minecraft:respawn_anchor");
        flees("minecraft:portal");
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "hoglin tests passed" << std::endl;
    return 0;
}
