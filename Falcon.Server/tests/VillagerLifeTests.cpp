#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/CelebrateSurviveGoal.h"
#include "World/Actor/AI/Goal/HideGoal.h"
#include "World/Actor/AI/Goal/MoveIndoorsGoal.h"
#include "World/Actor/AI/Goal/PlayGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *const SCHEDULE_GROUPS[] = {"basic_schedule", "child_schedule", "work_schedule", "farmer_schedule",
                                           "fisher_schedule", "librarian_schedule", "jobless_schedule",
                                           "wander_schedule_villager", "gather_schedule_villager",
                                           "home_schedule_villager", "bed_schedule_villager", "play_schedule_villager",
                                           "work_schedule_villager", "baby"};

    struct Arena {
        ServerNetworkHandler mOwner{"VillagerLife", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++) {
                    LevelChunk &chunk = mLevel->insertChunk(LevelChunk(x, z));
                    chunk.setPopulated(true);
                    chunk.setBiome(BiomeIds::PLAINS);
                }
            }
            for (int32_t x = -10; x <= 10; ++x) {
                for (int32_t z = -10; z <= 10; ++z)
                    mLevel->setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &villager(const Vector3f &position, const char *age) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, "minecraft:villager_v2", position));
            require(mob != nullptr, "the villager did not spawn");
            mob->startDefinition(mOwner);
            for (const char *group: SCHEDULE_GROUPS)
                mob->removeComponentGroup(group);
            mob->addComponentGroup(age);
            mob->tick(mOwner);
            return *mob;
        }

        int32_t countRockets() {
            int32_t count = 0;
            for (const auto &entry: mOwner.getActors()) {
                if (std::string(entry.second->getTypeId()) == "minecraft:fireworks_rocket")
                    ++count;
            }
            return count;
        }
    };

    void survivorsSetOffFireworks(Arena &arena) {
        MobActor &villager = arena.villager(Vector3f(0.5f, 64.0f, 0.5f), "adult");
        villager.addComponentGroup("minecraft:celebrate");
        CelebrateSurviveGoal::Settings settings;
        settings.mDurationTicks = 100;
        settings.mFireworksMinTicks = 40;
        settings.mFireworksMaxTicks = 40;
        settings.mOnEnd = json::parse(R"({"event": "minecraft:stop_celebrating", "target": "self"})");
        CelebrateSurviveGoal goal(settings);
        require(goal.canUse(arena.mOwner, villager), "the celebration did not start");
        goal.start(arena.mOwner, villager);
        int32_t ticks = 0;
        while (goal.canContinueToUse(arena.mOwner, villager)) {
            goal.tick(arena.mOwner, villager);
            ++ticks;
        }
        require(ticks == 100, "the celebration did not last its duration");
        require(arena.countRockets() == 2, "the fireworks did not follow their interval");
        goal.stop(arena.mOwner, villager);
        require(!villager.hasComponentGroup("minecraft:celebrate"), "the end event did not fire");
    }

    void onlyRaidsHideVillagers(Arena &arena) {
        MobActor &villager = arena.villager(Vector3f(4.5f, 64.0f, 4.5f), "adult");
        HideGoal goal(0.3f, PoiType::Bed, 600);
        for (int32_t call = 0; call < 200; ++call)
            require(!goal.canUse(arena.mOwner, villager), "a villager hid with no raid");
        require(!HideGoal::isVillageRaided(villager), "a villager without a village counted as raided");
    }

    void roofsDecideOutdoors(Arena &arena) {
        MobActor &villager = arena.villager(Vector3f(-4.5f, 64.0f, -4.5f), "adult");
        MoveIndoorsGoal outdoors(MoveIndoorsGoal::Direction::Outdoors, 0.3f, 160);
        require(!outdoors.canUse(arena.mOwner, villager), "a villager under the sky went outdoors");
        arena.mLevel->setBlockState(-5, 67, -5, BlockState("minecraft:stone"));
        bool started = false;
        for (int32_t call = 0; call < 20 && !started; ++call)
            started = outdoors.canUse(arena.mOwner, villager);
        require(started, "a villager under a roof did not head outdoors");

        MoveIndoorsGoal indoors(MoveIndoorsGoal::Direction::Indoors, 0.3f, 160);
        require(!indoors.canUse(arena.mOwner, villager), "a dry villager went indoors");
    }

    void childrenPlay(Arena &arena) {
        MobActor &adult = arena.villager(Vector3f(6.5f, 64.0f, -6.5f), "adult");
        MobActor &child = arena.villager(Vector3f(7.5f, 64.0f, -6.5f), "baby");
        arena.villager(Vector3f(9.5f, 64.0f, -6.5f), "baby");
        PlayGoal::Settings settings;
        settings.mSpeed = 0.3f;
        settings.mFollowDistance = 2.0f;
        settings.mFriendSearchArea = Vector3f(6.0f, 3.0f, 6.0f);
        settings.mMaxPlayTicks = 40;
        settings.mRandomRange = 16;
        settings.mRandomHeight = 3;
        settings.mFriendFilters.push_back(json::parse(
                R"({"all_of": [{"subject": "other", "test": "is_family", "value": "villager"},
                               {"subject": "other", "test": "is_baby", "operator": "==", "value": true}]})"));
        PlayGoal goal(settings);
        require(!goal.canUse(arena.mOwner, adult), "an adult played");
        require(goal.canUse(arena.mOwner, child), "a child did not play");
        goal.start(arena.mOwner, child);
        goal.tick(arena.mOwner, child);
        require(!child.getNavigation().isDone(), "the child did not chase its friend");
        for (int32_t tick = 1; tick < 40; ++tick)
            goal.tick(arena.mOwner, child);
        require(!goal.canContinueToUse(arena.mOwner, child), "the play outlasted its duration");
        goal.stop(arena.mOwner, child);
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-villager-life-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        survivorsSetOffFireworks(arena);
        onlyRaidsHideVillagers(arena);
        roofsDecideOutdoors(arena);
        childrenPlay(arena);
        std::cout << "villager life checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
