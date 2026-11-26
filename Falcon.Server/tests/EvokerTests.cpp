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

// a test server does not listen, so its tick returns at once: the clock is moved by hand
class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    const int32_t TICKS_PER_SECOND = 20;
    const char *const EVOKER = "minecraft:evocation_illager";
    const char *const FANG = "minecraft:evocation_fang";
    const char *const VEX = "minecraft:vex";
    const char *const TRADER = "minecraft:wandering_trader";

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
        // held in the middle: a fang or vex whose spot is past the edge of the floor has no ground and never comes
        MobActor *mHeld = nullptr;

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

        // the target is kept healed at a fixed offset from the evoker: the spell picked depends on that distance
        void tick(Actor &target, const MobActor &evoker, float offset) {
            if (mHeld != nullptr)
                mHeld->setPosition(Vector3f(0.5f, 64.0f, 0.5f));
            const Vector3f &from = evoker.getPosition();
            target.setPosition(Vector3f(from.x + offset, from.y, from.z));
            target.setHealth(target.getMaxHealth());
            ServerTickProbe::advance(mOwner, 1);
            mOwner.getWorlds().getDefault().getOverworld().tickTime();
            PathFinder::tryReserveSearch(-1);
            mOwner.tickActors();
        }

        // ticks until something of the type appears; one cast in four is the vexes and their 17 s cooldown:
        // the waits are long so a run of them does not starve the spell under test
        std::vector<ServerActor *> until(const char *identifier, Actor &target, const MobActor &evoker, float offset,
                                         int32_t seconds) {
            for (int32_t tick = 0; tick < seconds * TICKS_PER_SECOND; tick++) {
                this->tick(target, evoker, offset);
                std::vector<ServerActor *> found = all(identifier);
                if (!found.empty())
                    return found;
            }
            return {};
        }
    };

    // a target within 3 blocks: the fangs come up in rings around the evoker.
    // a trader, not a player: the evoker flees a close player before it casts
    void ringsUpClose() {
        Arena arena;
        MobActor &trader = arena.spawn(TRADER, Vector3f(2.5f, 64.0f, 0.5f));
        MobActor &evoker = arena.spawn(EVOKER, Vector3f(0.5f, 64.0f, 0.5f));
        const std::vector<ServerActor *> fangs = arena.until(FANG, trader, evoker, 2.0f, 120);
        require(!fangs.empty(), "the evoker never cast fangs at a close target");
        for (const ServerActor *fang: fangs) {
            require(horizontalDistance(fang->getPosition(), evoker.getPosition()) < 4.0f,
                    "a fang of the close spell came up away from the evoker");
            require(fang->getOwnerUniqueId() == evoker.getUniqueId(), "a fang does not belong to its evoker");
        }
    }

    // a farther target, out of the evoker's flee range: a line of fangs runs out toward it
    void lineFarAway() {
        Arena arena;
        ServerPlayer &player = arena.player(1, Vector3f(10.5f, 64.0f, 0.5f));
        MobActor &evoker = arena.spawn(EVOKER, Vector3f(0.5f, 64.0f, 0.5f));
        arena.mHeld = &evoker;
        std::vector<ServerActor *> fangs = arena.until(FANG, player, evoker, 10.0f, 120);
        require(!fangs.empty(), "the evoker never cast fangs at a far target");
        // the line comes out one fang every tick: let a few of them come up
        for (int32_t tick = 0; tick < 6; tick++)
            arena.tick(player, evoker, 10.0f);
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
        ServerPlayer &player = arena.player(1, Vector3f(10.5f, 64.0f, 0.5f));
        MobActor &evoker = arena.spawn(EVOKER, Vector3f(0.5f, 64.0f, 0.5f));
        arena.mHeld = &evoker;
        std::vector<ServerActor *> vexes = arena.until(VEX, player, evoker, 10.0f, 300);
        require(!vexes.empty(), "the evoker never summoned vexes");
        // the vexes appear with the summon's delay: give the rest of the cast time
        for (int32_t tick = 0; tick < TICKS_PER_SECOND; tick++)
            arena.tick(player, evoker, 10.0f);
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
