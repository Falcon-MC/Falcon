#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/WardenGoals.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/WardenActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
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
        ServerNetworkHandler mOwner{"Warden goals", "", 1};
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
            ServerTickProbe::advance(mOwner, 1000);
        }

        WardenActor &warden(const Vector3f &position) {
            auto *warden = dynamic_cast<WardenActor *>(mOwner.spawnActor(mLevel, WardenActor::IDENTIFIER, position));
            require(warden != nullptr, "the warden did not spawn");
            warden->setOnGround(true);
            return *warden;
        }

        MobActor &mob(const char *identifier, const Vector3f &position) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id("warden-goal-player", runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setGameType((int32_t) GameType::Survival);
            return player;
        }
    };

    Vector3f at(float x, float z) {
        return Vector3f(x, FLOOR_Y + 1.0f, z);
    }

    // the vanilla warden's own settings for the behavior
    std::unique_ptr<Goal> goalOf(const MobActor &mob, const std::string &behavior) {
        const json::Value *definition = EntityDefinitions::find(WardenActor::IDENTIFIER);
        require(definition != nullptr, "no warden definition");
        const json::Value *component = definition->get("components")->get("minecraft:behavior." + behavior);
        if (component == nullptr) {
            const json::Value *groups = definition->get("component_groups");
            component = groups->get("emerging")->get("minecraft:behavior." + behavior);
        }
        require(component != nullptr, "no minecraft:behavior." + behavior);
        std::unique_ptr<Goal> goal = WardenGoals::create(mob, behavior, *component);
        require(goal != nullptr, behavior + " did not make a goal");
        return goal;
    }

    // 7 seconds out of the ground, then minecraft:emerged swaps emerging for pushable
    void emerge() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        warden.fireEvent(arena.mOwner, "minecraft:spawn_emerging");
        std::unique_ptr<Goal> goal = goalOf(warden, "emerge");
        require(goal->canUse(arena.mOwner, warden), "emerge did not start");
        goal->start(arena.mOwner, warden);
        require(warden.getFlags().get(ActorFlag::Emerging), "emerging was not flagged");
        ServerTickProbe::advance(arena.mOwner, 140);
        require(goal->canContinueToUse(arena.mOwner, warden), "emerge ended before 7 seconds");
        ServerTickProbe::advance(arena.mOwner);
        require(!goal->canContinueToUse(arena.mOwner, warden), "emerge outlasted 7 seconds");
        goal->stop(arena.mOwner, warden);
        require(!warden.getFlags().get(ActorFlag::Emerging), "emerging stayed flagged");
        require(warden.hasComponentGroup("pushable") && !warden.hasComponentGroup("emerging"),
                "on_done did not fire minecraft:emerged");
    }

    // a minute alone and it digs away; a name, a vibration or a suspicion keeps it
    void dig() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        std::unique_ptr<Goal> goal = goalOf(warden, "dig");
        require(!goal->canUse(arena.mOwner, warden), "dig started before its idle time");
        ServerTickProbe::advance(arena.mOwner, 1200);
        require(goal->canUse(arena.mOwner, warden), "dig did not start after its idle time");

        warden.setNameTag("Sir");
        require(!goal->canUse(arena.mOwner, warden), "a named warden dug");
        warden.setNameTag("");

        warden.setSuspectPosition(Vector3i(3, FLOOR_Y + 1, 3), arena.mOwner.getCurrentTick());
        require(!goal->canUse(arena.mOwner, warden), "a recent suspicion did not keep it from digging");
        ServerTickProbe::advance(arena.mOwner, 1200);
        require(goal->canUse(arena.mOwner, warden), "an old suspicion still kept it");

        goal->start(arena.mOwner, warden);
        require(warden.getFlags().get(ActorFlag::Digging), "digging was not flagged");
        require(!warden.hasComponentGroup("pushable"), "on_digging_event did not remove pushable");
        ServerTickProbe::advance(arena.mOwner, 111);
        require(!goal->canContinueToUse(arena.mOwner, warden), "dig outlasted 5.5 seconds");
        goal->stop(arena.mOwner, warden);
        require(warden.isExpired(), "the warden did not leave after digging");
    }

    // angry at the pig: roars, then targets it 20 angrier
    void roar() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        MobActor &pig = arena.mob("minecraft:pig", at(4.5f, 0.5f));
        std::unique_ptr<Goal> goal = goalOf(warden, "roar");
        warden.getAngerLevel().increase(arena.mOwner, warden, pig, 79, false);
        require(!goal->canUse(arena.mOwner, warden), "it roared below angry_threshold");
        warden.getAngerLevel().increase(arena.mOwner, warden, pig, 1, false);
        require(goal->canUse(arena.mOwner, warden), "it did not roar at angry_threshold");
        goal->start(arena.mOwner, warden);
        require(warden.getFlags().get(ActorFlag::Roaring), "roaring was not flagged");
        goal->stop(arena.mOwner, warden);
        require(warden.getTarget(arena.mOwner) == &pig, "the roared at pig did not become the target");
        require(warden.getAngerLevel().getAngerAt(pig.getUniqueId()) == 100, "the roar did not add angry_boost");
    }

    // a player over a closer pig; suspected, and angered only within the cylinder
    void sniff() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        arena.mob("minecraft:pig", at(2.5f, 0.5f));
        ServerPlayer &player = arena.player(9100, at(10.5f, 0.5f));
        std::unique_ptr<Goal> goal = goalOf(warden, "sniff");
        auto *sniff = dynamic_cast<SniffGoal *>(goal.get());
        require(sniff->findCandidate(arena.mOwner, warden) == &player, "a closer pig went before the player");
        require(goal->canUse(arena.mOwner, warden), "sniff did not start");
        goal->start(arena.mOwner, warden);
        require(warden.getFlags().get(ActorFlag::Sniffing), "sniffing was not flagged");
        goal->stop(arena.mOwner, warden);
        require(warden.getSuspectPosition() == Vector3i(10, FLOOR_Y + 1, 0), "the sniffed player was not suspected");
        require(warden.getAngerLevel().getAngerAt(player.getUniqueId()) == 0,
                "a player past suspicion_radius_horizontal was angered");
        require(!goal->canUse(arena.mOwner, warden), "sniff started again inside its cooldown");

        warden.clearSuspectPosition();
        player.setPosition(at(4.5f, 0.5f));
        ServerTickProbe::advance(arena.mOwner, 200);
        require(goal->canUse(arena.mOwner, warden), "sniff did not start after its cooldown");
        goal->start(arena.mOwner, warden);
        goal->stop(arena.mOwner, warden);
        require(warden.getAngerLevel().getAngerAt(player.getUniqueId()) == 35,
                "a player within the cylinder was not angered");
    }

    // walks to the suspect block and forgets it there
    void investigate() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        std::unique_ptr<Goal> goal = goalOf(warden, "investigate_suspicious_location");
        require(!goal->canUse(arena.mOwner, warden), "it investigated with nothing suspect");
        warden.setSuspectPosition(Vector3i(0, FLOOR_Y + 1, 0), arena.mOwner.getCurrentTick());
        require(!goal->canUse(arena.mOwner, warden), "it investigated the block it stands on");
        warden.setSuspectPosition(Vector3i(8, FLOOR_Y + 1, 0), arena.mOwner.getCurrentTick());
        require(goal->canUse(arena.mOwner, warden), "it did not investigate a far suspect block");
        goal->start(arena.mOwner, warden);
        require(goal->canContinueToUse(arena.mOwner, warden), "it stopped at once");
        warden.setPosition(at(8.5f, 0.5f));
        warden.getNavigation().stop(warden);
        goal->tick(arena.mOwner, warden);
        require(!goal->canContinueToUse(arena.mOwner, warden), "it did not stop on the suspect block");
        goal->stop(arena.mOwner, warden);
        require(!warden.getSuspectPosition().has_value(), "the suspect block was not forgotten");
    }

    // charged for 1.7 seconds, one hit of 10 through the line, then 2 seconds of cooldown
    void sonicBoom() {
        Arena arena;
        WardenActor &warden = arena.warden(at(0.5f, 0.5f));
        MobActor &zombie = arena.mob("minecraft:zombie", at(10.5f, 0.5f));
        std::unique_ptr<Goal> goal = goalOf(warden, "sonic_boom");
        require(!goal->canUse(arena.mOwner, warden), "it boomed with no target");
        warden.setTarget(arena.mOwner, zombie.getRuntimeId());
        require(goal->canUse(arena.mOwner, warden), "it did not boom a target in range");
        zombie.setPosition(at(16.5f, 0.5f));
        require(!goal->canUse(arena.mOwner, warden), "it boomed past attack_range_horizontal");
        zombie.setPosition(at(10.5f, 0.5f));

        const float health = zombie.getHealth();
        goal->start(arena.mOwner, warden);
        require(warden.getFlags().get(ActorFlag::SonicBoom), "sonic boom was not flagged");
        ServerTickProbe::advance(arena.mOwner, 34);
        goal->tick(arena.mOwner, warden);
        require(zombie.getHealth() == health, "it hit before the charge ended");
        ServerTickProbe::advance(arena.mOwner);
        goal->tick(arena.mOwner, warden);
        require(zombie.getHealth() < health, "the boom did not hurt");
        require(zombie.getMotion().x > 0.0f, "the boom did not push the zombie away");
        const float hit = zombie.getHealth();
        ServerTickProbe::advance(arena.mOwner);
        goal->tick(arena.mOwner, warden);
        require(zombie.getHealth() == hit, "it hit twice in one use");

        goal->stop(arena.mOwner, warden);
        require(!warden.getFlags().get(ActorFlag::SonicBoom), "sonic boom stayed flagged");
        ServerTickProbe::advance(arena.mOwner, 40);
        require(!goal->canUse(arena.mOwner, warden), "it boomed inside attack_cooldown");
        ServerTickProbe::advance(arena.mOwner);
        require(goal->canUse(arena.mOwner, warden), "it did not boom after attack_cooldown");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-warden-goals-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        emerge();
        dig();
        roar();
        sniff();
        investigate();
        sonicBoom();
        std::cout << "warden goal checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
