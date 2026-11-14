#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/MoveTowardsTargetGoal.h"
#include "World/Actor/AI/Goal/OfferFlowerGoal.h"
#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Neutral/GolemBuilding.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Village.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Iron golem", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, "mob did not spawn");
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }
    };

    void attackDamage(Arena &arena) {
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(0.5f, 100.0f, 0.5f));
        bool lowest = false;
        bool highest = false;
        for (int32_t roll = 0; roll < 4000; ++roll) {
            const float damage = golem.getAttackDamage(Difficulty::Normal);
            require(damage >= 7.0f && damage <= 21.0f, "iron golem damage left its 7 to 21 range");
            lowest = lowest || damage == 7.0f;
            highest = highest || damage == 21.0f;
        }
        require(lowest && highest, "iron golem damage never reached an end of its range");
        require(golem.getAttackDamage(Difficulty::Peaceful) == 0.0f, "iron golem hurts on peaceful");
    }

    void ingotRepair(Arena &arena) {
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(4.5f, 100.0f, 0.5f));
        ServerPlayer &player = arena.player(9301, Vector3f(5.5f, 100.0f, 0.5f));

        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:iron_ingot", 2));
        require(!golem.onInteract(arena.mOwner, player), "a healthy golem took the ingot");
        require(player.getInventory().getItemInHand().mCount == 2, "a healthy golem used the ingot");

        golem.setHealth(50.0f);
        require(golem.onInteract(arena.mOwner, player), "the ingot did not repair the golem");
        require(golem.getHealth() == 75.0f, "the ingot did not restore 25 health");
        require(player.getInventory().getItemInHand().mCount == 1, "the repair did not use the ingot");

        golem.setHealth(90.0f);
        require(golem.onInteract(arena.mOwner, player), "the last ingot did not repair the golem");
        require(golem.getHealth() == 100.0f, "the repair overflowed the maximum health");
    }

    void groundSnap(Arena &arena) {
        Level &level = *arena.mLevel;
        for (int32_t x = -20; x <= -10; ++x) {
            for (int32_t z = -5; z <= 5; ++z)
                level.setBlockState(x, 60, z, BlockState("minecraft:stone"));
        }

        Vector3i ground;
        require(RandomGroundPosition::snapToGround(level, Vector3i(-15, 60, 0), 55, 65, ground)
                        && ground.y == 61,
                "a point inside the floor did not snap on top of it");
        require(RandomGroundPosition::snapToGround(level, Vector3i(-15, 64, 0), 57, 67, ground) && ground.y == 61,
                "a point above the floor did not fall onto it");
        require(!RandomGroundPosition::snapToGround(level, Vector3i(-15, 70, 0), 66, 74, ground),
                "a point out of vertical reach found the floor");
    }

    void moveTowardsTarget(Arena &arena) {
        Level &level = *arena.mLevel;
        // a floor wide enough for every point the search can draw towards +x
        for (int32_t x = 8; x <= 47; ++x) {
            for (int32_t z = -16; z <= 16; ++z)
                level.setBlockState(x, 70, z, BlockState("minecraft:stone"));
        }
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(10.5f, 71.0f, 0.5f));
        MobActor &skeleton = arena.spawn("minecraft:skeleton", Vector3f(36.5f, 71.0f, 0.5f));
        require(golem.setTarget(arena.mOwner, skeleton.getRuntimeId()), "could not aim the golem");

        MoveTowardsTargetGoal goal(0.25f, 32.0f);
        require(goal.canUse(arena.mOwner, golem), "a target 26 blocks away was not chased");
        skeleton.setPosition(Vector3f(43.5f, 71.0f, 0.5f));
        require(!goal.canUse(arena.mOwner, golem), "a target 33 blocks away was chased");
    }

    void offerFlower(Arena &arena) {
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(0.5f, 100.0f, 30.5f));
        OfferFlowerGoal::Settings settings;
        settings.mChanceToStart = 1.0f;
        settings.mDurationTicks = 400;
        settings.mSearchArea = Vector3f(6.0f, 2.0f, 6.0f);
        OfferFlowerGoal goal(settings);
        require(!goal.canUse(arena.mOwner, golem), "a flower was offered with nobody to take it");

        MobActor &child = arena.spawn("minecraft:villager_v2", Vector3f(4.5f, 100.0f, 30.5f));
        child.addComponentGroup("baby");
        require(goal.canUse(arena.mOwner, golem), "the child next to the golem was not offered a flower");
        goal.start(arena.mOwner, golem);
        require(goal.canContinueToUse(arena.mOwner, golem), "the offer ended at once");
        goal.stop(arena.mOwner, golem);

        child.setPosition(Vector3f(12.5f, 100.0f, 30.5f));
        require(!goal.canUse(arena.mOwner, golem), "a child beyond the search area was offered a flower");
    }

    void villageAggression(Arena &arena) {
        Village village;
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(0.5f, 100.0f, -30.5f));
        MobActor &skeleton = arena.spawn("minecraft:skeleton", Vector3f(10.5f, 100.0f, -30.5f));
        ServerPlayer &player = arena.player(9401, Vector3f(4.5f, 100.0f, -30.5f));

        require(village.findClosestAggressor(arena.mOwner, golem) == nullptr, "a quiet village had an aggressor");
        village.addAggressor(skeleton.getRuntimeId(), 1000);
        require(village.findClosestAggressor(arena.mOwner, golem) == &skeleton, "the aggressor was forgotten");
        village.addAggressor(player.getRuntimeId(), 1000);
        require(village.findClosestAggressor(arena.mOwner, golem) == &player, "the closer aggressor lost");

        // hitting a villager costs 1 standing: the golem only turns on a player at -15
        for (int32_t hit = 0; hit < 14; ++hit)
            village.changeStanding(player.getUniqueId(), -1);
        require(village.findClosestHostilePlayer(arena.mOwner, golem) == nullptr,
                "-14 standing made the village hostile");
        village.changeStanding(player.getUniqueId(), -1);
        require(village.findClosestHostilePlayer(arena.mOwner, golem) == &player,
                "-15 standing left the village calm");

        village.changeStanding(player.getUniqueId(), -100);
        require(village.getStanding(player.getUniqueId()) == -30, "standing went below -30");
        village.changeStanding(player.getUniqueId(), 100);
        require(village.getStanding(player.getUniqueId()) == 10, "standing went above 10");

        // remembered for 300 ticks after the hit
        village.tick(arena.mOwner, *arena.mLevel, 1300);
        require(village.findClosestAggressor(arena.mOwner, golem) == &player, "the aggressor left before 300 ticks");
        village.tick(arena.mOwner, *arena.mLevel, 1301);
        require(village.findClosestAggressor(arena.mOwner, golem) == nullptr, "the aggressor stayed past 300 ticks");

        // a negative standing climbs back by 1 every 12000 ticks
        village.changeStanding(player.getUniqueId(), -25);
        village.tick(arena.mOwner, *arena.mLevel, 1320);
        require(village.getStanding(player.getUniqueId()) == -14, "the first decay pass did not raise the standing");
        village.tick(arena.mOwner, *arena.mLevel, 1350);
        require(village.getStanding(player.getUniqueId()) == -14, "the standing rose before 12000 ticks");
        village.tick(arena.mOwner, *arena.mLevel, 13320);
        require(village.getStanding(player.getUniqueId()) == -13, "the standing did not rise after 12000 ticks");
    }

    size_t countOf(Arena &arena, const char *identifier) {
        size_t found = 0;
        for (auto &entry: arena.mOwner.getActors()) {
            if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                ++found;
        }
        return found;
    }

    void buildGolems(Arena &arena) {
        Level &level = *arena.mLevel;
        const BlockState iron("minecraft:iron_block");
        const BlockState pumpkin("minecraft:carved_pumpkin");

        // an iron golem along x: the arms and the body, then the head on top
        const int32_t x = -20;
        const int32_t y = 110;
        const int32_t z = -20;
        level.setBlockState(x, y, z, iron);
        level.setBlockState(x - 1, y + 1, z, iron);
        level.setBlockState(x, y + 1, z, iron);
        level.setBlockState(x + 1, y + 1, z, iron);
        level.setBlockState(x, y + 2, z, pumpkin);
        const GolemBuilding::Match match = GolemBuilding::find(level, Vector3i(x, y + 2, z));
        require(match.mKind == GolemBuilding::Kind::Iron, "the iron golem pattern was not found");
        require(match.mOrigin == Vector3i(x - 1, y + 2, z) && match.mForwards == 3 && match.mUp == 0,
                "the iron golem pattern matched from the wrong corner");

        const size_t golems = countOf(arena, "minecraft:iron_golem");
        require(GolemBuilding::tryBuild(arena.mOwner, level, Vector3i(x, y + 2, z)), "the iron golem was not built");
        require(countOf(arena, "minecraft:iron_golem") == golems + 1, "no iron golem appeared");
        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dy = 0; dy <= 2; ++dy)
                require(level.getBlockState(x + dx, y + dy, z).mName == "minecraft:air", "a golem block stayed");
        }

        MobActor *built = nullptr;
        for (auto &entry: arena.mOwner.getActors()) {
            const Vector3f &position = entry.second->getPosition();
            if (entry.second->getTypeId() == "minecraft:iron_golem" && position.x == (float) x
                && position.y == (float) y && position.z == (float) (z + 1))
                built = dynamic_cast<MobActor *>(entry.second.get());
        }
        require(built != nullptr, "the iron golem is not on the corner of its bottom block");
        built->startDefinition(arena.mOwner);
        require(built->hasComponentGroup("minecraft:player_created"), "a built golem is not player created");

        // a block where an empty corner should be spoils the shape
        level.setBlockState(x, y, z, iron);
        level.setBlockState(x - 1, y + 1, z, iron);
        level.setBlockState(x, y + 1, z, iron);
        level.setBlockState(x + 1, y + 1, z, iron);
        level.setBlockState(x - 1, y, z, BlockState("minecraft:stone"));
        level.setBlockState(x, y + 2, z, pumpkin);
        require(GolemBuilding::find(level, Vector3i(x, y + 2, z)).mKind == GolemBuilding::Kind::None,
                "an iron golem matched with a filled corner");

        // a snow golem: two snow blocks under a jack o'lantern
        const int32_t snowX = -25;
        level.setBlockState(snowX, y, z, BlockState("minecraft:snow"));
        level.setBlockState(snowX, y + 1, z, BlockState("minecraft:snow"));
        level.setBlockState(snowX, y + 2, z, BlockState("minecraft:lit_pumpkin"));
        const size_t snowGolems = countOf(arena, "minecraft:snow_golem");
        require(GolemBuilding::tryBuild(arena.mOwner, level, Vector3i(snowX, y + 2, z)),
                "the snow golem was not built");
        require(countOf(arena, "minecraft:snow_golem") == snowGolems + 1, "no snow golem appeared");
        require(level.getBlockState(snowX, y + 1, z).mName == "minecraft:air", "a snow golem block stayed");
    }

    void villageGolemCondition(Arena &arena) {
        Village village;
        const PoiBlueprint bed{"minecraft:bed", "", "", PoiType::Bed, 1, 0};
        std::vector<std::shared_ptr<PoiInstance>> beds;
        for (int32_t index = 0; index < 20; ++index) {
            beds.push_back(std::make_shared<PoiInstance>(Vector3i(index, 64, 40), bed));
            require(village.addPoi(beds.back()), "a bed was refused");
        }

        std::vector<int64_t> villagers;
        for (int32_t index = 0; index < 10; ++index) {
            MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f((float) index + 0.5f, 65.0f, 40.5f));
            villagers.push_back(villager.getUniqueId());
            village.addDweller(arena.mOwner, DwellerRole::Inhabitant, villager.getUniqueId());
        }
        require(village.countBeds() == 20 && village.getInhabitantCount() == 10, "the village was not set up");

        // three quarters of the ten villagers must have worked within a day
        const int64_t now = 50000;
        for (int32_t index = 0; index < 7; ++index)
            village.recordWork(villagers[(size_t) index], now - 100);
        require(!village.wantsGolem(now), "seven workers out of ten called a golem");
        village.recordWork(villagers[7], now - 100);
        require(village.wantsGolem(now), "eight workers out of ten did not call a golem");
        require(!village.wantsGolem(now + 23900), "a day old work still counted");

        village.recordWork(424242, now);
        require(village.countRecentWorkers(now) == 8, "a stranger's work was recorded");

        // one defender for ten inhabitants is enough
        village.addDweller(arena.mOwner, DwellerRole::Defender, 777);
        require(!village.wantsGolem(now), "a village with its golem called another");
        village.removeDweller(DwellerRole::Defender, 777);

        // a golem per ten inhabitants: nine never call one
        village.removeDweller(DwellerRole::Inhabitant, villagers[9]);
        require(!village.wantsGolem(now), "nine villagers called a golem");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-iron-golem-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        attackDamage(arena);
        ingotRepair(arena);
        groundSnap(arena);
        buildGolems(arena);
        moveTowardsTarget(arena);
        offerFlower(arena);
        villageAggression(arena);
        villageGolemCondition(arena);
        std::cout << "iron golem checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
