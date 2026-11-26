#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const int32_t TICKS_PER_SECOND = 20;
    const char *const EVOKER = "minecraft:evocation_illager";
    const char *const FANG = "minecraft:evocation_fang";
    const char *const VEX = "minecraft:vex";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    float horizontalDistance(const Vector3f &a, const Vector3f &b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Evokers", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -28; x <= 28; x++) {
                for (int32_t z = -28; z <= 28; z++)
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

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent("minecraft:entity_spawned"); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        std::vector<ServerActor *> all(const char *identifier) {
            std::vector<ServerActor *> found;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    found.push_back(entry.second.get());
            }
            return found;
        }

        // ticks until something of the type appears; the player stands still and is healed, as a client would
        std::vector<ServerActor *> until(const char *identifier, ServerPlayer &player, const Vector3f &at,
                                         int32_t seconds) {
            for (int32_t tick = 0; tick < seconds * TICKS_PER_SECOND; tick++) {
                player.setPosition(at);
                player.setHealth(player.getMaxHealth());
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                PathFinder::tryReserveSearch(-1);
                mOwner.tickActors();
                std::vector<ServerActor *> found = all(identifier);
                if (!found.empty())
                    return found;
            }
            return {};
        }
    };

    // a target within 3 blocks: the fangs come up in rings around the evoker
    void ringsUpClose() {
        Arena arena;
        const Vector3f at(2.5f, 64.0f, 0.5f);
        ServerPlayer &player = arena.player(1, at);
        MobActor &evoker = arena.spawn(EVOKER, Vector3f(0.5f, 64.0f, 0.5f));
        const std::vector<ServerActor *> fangs = arena.until(FANG, player, at, 30);
        require(!fangs.empty(), "the evoker never cast fangs at a close target");
        for (const ServerActor *fang: fangs) {
            require(horizontalDistance(fang->getPosition(), evoker.getPosition()) < 4.0f,
                    "a fang of the close spell came up away from the evoker");
            require(fang->getOwnerUniqueId() == evoker.getUniqueId(), "a fang does not belong to its evoker");
        }
    }

    // a farther target: a line of fangs runs out toward it
    void lineFarAway() {
        Arena arena;
        const Vector3f at(10.5f, 64.0f, 0.5f);
        ServerPlayer &player = arena.player(1, at);
        MobActor &evoker = arena.spawn(EVOKER, Vector3f(0.5f, 64.0f, 0.5f));
        std::vector<ServerActor *> fangs = arena.until(FANG, player, at, 30);
        require(!fangs.empty(), "the evoker never cast fangs at a far target");
        // the line comes out one fang every tick: let a few of them come up
        for (int32_t tick = 0; tick < 6; tick++) {
            player.setPosition(at);
            player.setHealth(player.getMaxHealth());
            arena.mOwner.tickActors();
        }
        fangs = arena.all(FANG);
        float farthest = 0.0f;
        for (const ServerActor *fang: fangs) {
            const Vector3f position = fang->getPosition();
            require(std::fabs(position.z - evoker.getPosition().z) < 1.5f, "a fang left the line toward the target");
            farthest = std::max(farthest, horizontalDistance(position, evoker.getPosition()));
        }
        require(fangs.size() > 1 && farthest > 3.0f, "the far spell did not run out as a line");
    }

    // vexes come three at a time, owned by the evoker and with their life running out
    void summonsVexes() {
        Arena arena;
        const Vector3f at(10.5f, 64.0f, 0.5f);
        ServerPlayer &player = arena.player(1, at);
        MobActor &evoker = arena.spawn(EVOKER, Vector3f(0.5f, 64.0f, 0.5f));
        std::vector<ServerActor *> vexes = arena.until(VEX, player, at, 120);
        require(!vexes.empty(), "the evoker never summoned vexes");
        // the vexes appear with the summon's delay: give the rest of the cast time
        for (int32_t tick = 0; tick < TICKS_PER_SECOND; tick++)
            arena.mOwner.tickActors();
        vexes = arena.all(VEX);
        require(vexes.size() == 3, "one cast summoned " + std::to_string(vexes.size()) + " vexes");
        for (ServerActor *actor: vexes) {
            MobActor *vex = dynamic_cast<MobActor *>(actor);
            require(vex != nullptr && vex->getOwnerUniqueId() == evoker.getUniqueId(), "a vex has no evoker");
            require(vex->hasComponentGroup("minecraft:start_damage_timer"), "a summoned vex lives forever");
        }

        // a vex spawned any other way keeps its life
        MobActor &egg = arena.spawn(VEX, Vector3f(-6.5f, 66.0f, 0.5f));
        require(!egg.hasComponentGroup("minecraft:start_damage_timer"), "a vex from nowhere loses its life");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        ringsUpClose();
        lineFarAway();
        summonsVexes();
    } catch (const std::exception &error) {
        std::clog << "Evoker regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "evoker checks passed\n";
    return 0;
}
