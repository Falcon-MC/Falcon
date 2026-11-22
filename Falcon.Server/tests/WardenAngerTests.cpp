#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/Molang.h"
#include "World/Actor/Mob/Hostile/WardenActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks = 1) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    const int32_t FLOOR_Y = 99;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Warden anger", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            mLevel.setOwner(&mOwner);
            for (int32_t x = -3; x <= 3; x++) {
                for (int32_t z = -3; z <= 3; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            const BlockState stone("minecraft:stone");
            for (int32_t x = -40; x < 40; x++) {
                for (int32_t z = -40; z < 40; z++)
                    mLevel.setBlockState(x, FLOOR_Y, z, stone);
            }
            // well past the first projectile window
            ServerTickProbe::advance(mOwner, 1000);
        }

        WardenActor &warden(const Vector3f &position) {
            auto *warden = dynamic_cast<WardenActor *>(mOwner.spawnActor(mLevel, WardenActor::IDENTIFIER, position));
            require(warden != nullptr, "the warden did not spawn");
            return *warden;
        }

        MobActor &mob(const char *identifier, const Vector3f &position) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position, GameType gameType) {
            const NetworkIdentifier id("warden-player", runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setGameType((int32_t) gameType);
            return player;
        }
    };

    Vector3f at(float x, float z) {
        return Vector3f(x, FLOOR_Y + 1.0f, z);
    }

    // the game's table: the angry first, players first among equals, then the angrier
    void angerTable() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        MobActor &pig = arena.mob("minecraft:pig", at(3.5f, 0.5f));
        ServerPlayer &player = arena.player(9001, at(-3.5f, 0.5f), GameType::Survival);
        MobAngerLevel &anger = warden.getAngerLevel();

        anger.increase(arena.mOwner, warden, pig, 50, false);
        anger.increase(arena.mOwner, warden, player, 35, false);
        require(anger.getNuisances().size() == 2, "two nuisances were not both remembered");
        require(anger.getNuisances().front().mActorId == player.getUniqueId(),
                "among the calm a player did not come first");
        require(anger.getLevel() == 50, "the level is not the highest anger");
        require(Molang::evaluate("query.anger_level(this)", warden).mNumber == 50.0, "query.anger_level is wrong");

        anger.increase(arena.mOwner, warden, pig, 40, false);
        require(anger.getNuisances().front().mActorId == pig.getUniqueId(),
                "an angry pig did not go before a calm player");
        anger.increase(arena.mOwner, warden, pig, 500, false);
        require(anger.getAngerAt(pig.getUniqueId()) == 150, "the anger went past max_anger");

        // a second of anger_decrement_interval takes one point from each
        ServerTickProbe::advance(arena.mOwner);
        anger.tick(arena.mOwner, warden);
        ServerTickProbe::advance(arena.mOwner, 20);
        anger.tick(arena.mOwner, warden);
        require(anger.getAngerAt(pig.getUniqueId()) == 149 && anger.getAngerAt(player.getUniqueId()) == 34,
                "anger did not cool down by one a second");

        // a creative player is no target: dropped at the next decrement
        player.setGameType((int32_t) GameType::Creative);
        ServerTickProbe::advance(arena.mOwner, 20);
        anger.tick(arena.mOwner, warden);
        require(anger.getAngerAt(player.getUniqueId()) == 0, "a creative player kept the warden angry");

        Tag saved = Tag::ofCompound();
        anger.save(saved);
        MobAngerLevel loaded;
        loaded.load(saved);
        require(loaded.getAngerAt(pig.getUniqueId()) == anger.getAngerAt(pig.getUniqueId()),
                "Nuisances did not load back");
    }

    // hit, touched or heard: the game's amounts, and where the warden goes and looks
    void disturbances() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        MobActor &pig = arena.mob("minecraft:pig", at(2.5f, 0.5f));
        ServerPlayer &shooter = arena.player(9002, at(-10.5f, 0.5f), GameType::Survival);
        MobAngerLevel &anger = warden.getAngerLevel();

        // touched: 35 and the block it was touched from, once a second
        warden.onPushedBy(arena.mOwner, pig.getUniqueId());
        require(anger.getAngerAt(pig.getUniqueId()) == 35, "a touch did not add default_annoyingness");
        require(warden.getSuspectPosition() == Vector3i(2, FLOOR_Y + 1, 0), "a touch did not set the suspect position");
        warden.onPushedBy(arena.mOwner, pig.getUniqueId());
        require(anger.getAngerAt(pig.getUniqueId()) == 35, "a second touch within a second counted");

        // a vibration made by the pig: 35 more, and it goes and looks where it came from
        warden.receiveVibration(arena.mOwner, Vector3i(5, FLOOR_Y + 1, 5), &pig, nullptr);
        require(anger.getAngerAt(pig.getUniqueId()) == 70, "a vibration did not add default_annoyingness");
        require(warden.getSuspectPosition() == Vector3i(5, FLOOR_Y + 1, 5),
                "a calm warden did not suspect the vibration");

        // a projectile: 10 for its owner, 35 and the owner's own block for one soon after
        warden.receiveVibration(arena.mOwner, Vector3i(1, FLOOR_Y + 1, 1), nullptr, &shooter);
        require(anger.getAngerAt(shooter.getUniqueId()) == 10,
                "a first projectile did not add the projectile annoyingness");
        require(warden.getSuspectPosition() == Vector3i(1, FLOOR_Y + 1, 1),
                "a first projectile did not point at itself");
        ServerTickProbe::advance(arena.mOwner, 50);
        warden.receiveVibration(arena.mOwner, Vector3i(1, FLOOR_Y + 1, 1), nullptr, &shooter);
        require(anger.getAngerAt(shooter.getUniqueId()) == 45,
                "a second projectile did not add default_annoyingness");
        require(warden.getSuspectPosition() == Vector3i(-11, FLOOR_Y + 1, 0),
                "a second projectile did not give away its owner");

        // hit: threshold and boost at once, and the attacker becomes the target
        warden.onDamaged(arena.mOwner, &pig);
        require(anger.getAngerAt(pig.getUniqueId()) == 150, "a hit did not add angry_threshold and angry_boost");
        require(warden.getTarget(arena.mOwner) == &pig, "the attacker did not become the target");
    }

    // a target the warden has cooled down on is dropped
    void calmTargetDropped() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        MobActor &pig = arena.mob("minecraft:pig", at(2.5f, 0.5f));
        warden.getAngerLevel().increase(arena.mOwner, warden, pig, 40, false);
        warden.setTarget(arena.mOwner, pig.getRuntimeId());
        warden.getAngerLevel().tick(arena.mOwner, warden);
        require(warden.getTarget(arena.mOwner) == nullptr, "the warden kept a target below angry_threshold");
    }

    // a block placed next to it reaches the warden through its listener, two seconds apart at most
    void hearsTheWorld() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        MobActor &pig = arena.mob("minecraft:pig", at(6.5f, 0.5f));
        GameEventContext context;
        context.mSource = &pig;
        require(warden.listensTo(arena.mOwner, context), "the warden did not listen");
        require(!warden.listensTo(arena.mOwner, context), "the warden listened again inside its cooldown");
        ServerTickProbe::advance(arena.mOwner, WardenActor::VIBRATION_COOLDOWN);

        arena.mLevel.postGameEvent(GameEvents::BLOCK_PLACE, Vector3f(6.5f, FLOOR_Y + 1.5f, 0.5f), &pig);
        for (int32_t tick = 0; tick < 20; tick++) {
            ServerTickProbe::advance(arena.mOwner);
            arena.mOwner.tickActors();
        }
        require(warden.getLastVibration() == Vector3i(6, FLOOR_Y + 1, 0), "the vibration never reached the warden");
        require(warden.getAngerLevel().getAngerAt(pig.getUniqueId()) == 35, "the heard pig did not anger the warden");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-warden-anger-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        angerTable();
        disturbances();
        calmTargetDropped();
        hearsTheWorld();
        std::cout << "warden anger checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
