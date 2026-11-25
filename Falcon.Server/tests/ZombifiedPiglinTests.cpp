#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Definition/EntityDefinitions.h"
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
        ServerNetworkHandler mOwner{"Zombified piglins", "", 1};
        Level &mLevel;

        explicit Arena(DimensionType dimension) : mLevel(mOwner.getWorlds().getDefault().getLevel(dimension)) {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -12; x <= 26; x++) {
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

        MobActor *first(const char *identifier) {
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    return dynamic_cast<MobActor *>(entry.second.get());
            }
            return nullptr;
        }
    };

    MobActor &adult(Arena &arena, const Vector3f &position) {
        return arena.spawn("minecraft:zombie_pigman", position, "spawn_adult");
    }

    // a blow angers the whole group in the box grown by broadcast_range 20, busy or not; each angered one passes it on
    void groupAnger() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &hit = adult(arena, Vector3f(0.5f, 64.0f, 2.5f));
        MobActor &near = adult(arena, Vector3f(19.5f, 64.0f, 2.5f));
        MobActor &corner = adult(arena, Vector3f(19.5f, 64.0f, 21.5f));
        MobActor &far = adult(arena, Vector3f(0.5f, 64.0f, -21.5f));
        MobActor &busy = adult(arena, Vector3f(-6.5f, 64.0f, 2.5f));
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(-10.5f, 64.0f, 2.5f), "minecraft:entity_spawned");
        // calm without its on_target_acquired: the pig does not make it angry and start a broadcast of its own
        busy.removeComponentGroup("minecraft:pig_zombie_calm");
        require(busy.setTarget(arena.mOwner, pig.getRuntimeId()), "the busy zombified piglin took no target");
        arena.ticks(2);
        require(!busy.hasComponentGroup("minecraft:pig_zombie_angry"), "the busy zombified piglin got angry at the pig");

        arena.mOwner.damageActor(hit, 1.0f, &player);
        arena.ticks(3);
        require(hit.getTarget(arena.mOwner) == &player && hit.hasComponentGroup("minecraft:pig_zombie_angry"),
                "the hurt zombified piglin is not angry at the player");
        require(near.getTarget(arena.mOwner) == &player, "a zombified piglin 19 blocks off stayed calm");
        require(corner.getTarget(arena.mOwner) == &player, "the box corner 19 blocks off on both axes stayed calm");
        require(busy.getTarget(arena.mOwner) == &player, "a zombified piglin chasing a pig kept the pig");
        require(far.getTarget(arena.mOwner) == nullptr, "a zombified piglin 24 blocks off was angered");

        // the anger is a component group: it survives a reload
        const Tag data = near.saveNbt();
        MobActor *loaded = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:zombie_pigman", Vector3f(30.5f, 64.0f, 30.5f)));
        require(loaded != nullptr, "no zombified piglin to load into");
        loaded->loadNbt(data);
        require(loaded->hasComponentGroup("minecraft:pig_zombie_angry"), "the anger was lost on reload");
    }

    // a bolt on a pig: no damage, then a zombified piglin after the half second delay
    void pigStruckByLightning() {
        Arena arena(DimensionType::Overworld);
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(0.5f, 64.0f, 0.5f), "minecraft:entity_spawned");
        arena.mOwner.strikeLightning(arena.mLevel, pig.getPosition());
        require(pig.hasComponentGroup("minecraft:pig_transform"), "the bolt did not start the pig's transformation");
        arena.ticks(5);
        require(arena.countOf("minecraft:zombie_pigman") == 0, "the pig converted before its half second");
        arena.ticks(10);
        require(arena.countOf("minecraft:pig") == 0 && arena.countOf("minecraft:zombie_pigman") == 1,
                "the pig did not become a zombified piglin");
        MobActor *zombified = arena.first("minecraft:zombie_pigman");
        require(zombified->hasComponentGroup("minecraft:pig_zombie_adult"), "the adult pig gave a baby");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        groupAnger();
        pigStruckByLightning();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "zombified piglin tests passed" << std::endl;
    return 0;
}
