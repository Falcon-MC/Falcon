#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/WorkGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/World.h"
#include "World/Village/PoiBlueprint.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Village.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *const SCHEDULE_GROUPS[] = {"basic_schedule",
                                           "child_schedule",
                                           "work_schedule",
                                           "farmer_schedule",
                                           "fisher_schedule",
                                           "librarian_schedule",
                                           "jobless_schedule",
                                           "wander_schedule_villager",
                                           "gather_schedule_villager",
                                           "home_schedule_villager",
                                           "bed_schedule_villager",
                                           "play_schedule_villager",
                                           "work_schedule_villager",
                                           "baby"};

    struct Arena {
        ServerNetworkHandler mOwner{"Villager", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -4; x <= 4; x++) {
                for (int32_t z = -4; z <= 4; z++) {
                    LevelChunk &chunk = mLevel->insertChunk(LevelChunk(x, z));
                    chunk.setPopulated(true);
                    chunk.setBiome(BiomeIds::PLAINS);
                }
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, "mob did not spawn");
            return *mob;
        }

        void floor(int32_t minX, int32_t maxX, int32_t y, int32_t minZ, int32_t maxZ) {
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z)
                    mLevel->setBlockState(x, y, z, BlockState("minecraft:stone"));
            }
        }

        // an adult without a job: its own groups decide the schedule, not the spawn rolls
        MobActor &adult(const Vector3f &position) {
            MobActor &villager = spawn("minecraft:villager_v2", position);
            villager.startDefinition(mOwner);
            for (const char *group: SCHEDULE_GROUPS)
                villager.removeComponentGroup(group);
            villager.addComponentGroup("adult");
            villager.addComponentGroup("unskilled");
            // syncs the baby flag with the groups
            villager.tick(mOwner);
            return villager;
        }
    };

    void scheduleFollowsTheClock(Arena &arena) {
        MobActor &villager = arena.adult(Vector3f(0.5f, 100.0f, 0.5f));
        villager.addComponentGroup("basic_schedule");

        arena.mLevel->setTime(9000);
        villager.tick(arena.mOwner);
        require(villager.hasComponentGroup("gather_schedule_villager"), "9000 did not gather the villager");

        // the event of a slot fires once: a removed group stays removed while the slot lasts
        villager.removeComponentGroup("gather_schedule_villager");
        villager.tick(arena.mOwner);
        require(!villager.hasComponentGroup("gather_schedule_villager"), "the gather event fired twice in its slot");

        arena.mLevel->setTime(13000);
        villager.tick(arena.mOwner);
        require(villager.hasComponentGroup("bed_schedule_villager"), "13000 did not send the villager to bed");
        require(!villager.hasComponentGroup("make_and_receive_love"), "the bed slot kept the breeding goals");

        // the clock wraps every day
        arena.mLevel->setTime(24000 * 3 + 500);
        villager.tick(arena.mOwner);
        require(villager.hasComponentGroup("wander_schedule_villager"), "a new morning did not wake the villager");
        require(!villager.hasComponentGroup("bed_schedule_villager"), "the morning kept the bed slot");

        // a job swaps the schedule: its first slot fires at once
        villager.addComponentGroup("work_schedule");
        villager.tick(arena.mOwner);
        require(villager.hasComponentGroup("work_schedule_villager"), "the work schedule did not start the shift");
    }

    struct WorkSite {
        std::shared_ptr<Village> mVillage;
        std::shared_ptr<PoiInstance> mPoi;
        MobActor *mVillager = nullptr;
    };

    WorkSite claimJobSite(Arena &arena, const Vector3i &site, const char *block) {
        WorkSite result;
        result.mVillage = std::make_shared<Village>();
        const PoiBlueprint *blueprint = PoiBlueprints::find(block);
        require(blueprint != nullptr, "the job site block has no blueprint");
        result.mPoi = std::make_shared<PoiInstance>(site, *blueprint);
        require(result.mVillage->addPoi(result.mPoi), "the job site was refused");

        arena.mLevel->setBlockState(site.x, site.y, site.z, BlockState(block));
        MobActor &villager = arena.adult(Vector3f((float) site.x + 0.5f, (float) site.y, (float) site.z + 2.5f));
        villager.getDweller().joinVillage(arena.mOwner, villager, *result.mVillage);
        // claims run on the ticks where tick % 30 == 15
        result.mVillage->tick(arena.mOwner, *arena.mLevel, 15);
        require(result.mVillage->getClaimedPoi(villager.getUniqueId(), PoiType::JobSite) == result.mPoi,
                "the villager did not claim the job site");
        result.mVillager = &villager;
        return result;
    }

    WorkGoal::Settings workSettings() {
        WorkGoal::Settings settings;
        settings.mSpeed = 0.25f;
        settings.mActiveTicks = 250;
        settings.mCooldownTicks = 200;
        settings.mRainToleranceTicks = 100;
        settings.mSoundDelayMin = 100;
        settings.mSoundDelayMax = 200;
        settings.mOnArrival = json::parse(R"({"event": "minecraft:resupply_trades", "target": "self"})");
        return settings;
    }

    // the goal looks for its point once every 60 to 119 calls, then on the next call
    bool waitForStart(Arena &arena, WorkGoal &goal, MobActor &mob) {
        for (int32_t call = 0; call < 125; ++call) {
            if (goal.canUse(arena.mOwner, mob))
                return true;
        }
        return false;
    }

    void workCountsForTheVillage(Arena &arena) {
        arena.floor(20, 30, 63, 20, 30);
        const Vector3i site(24, 64, 24);
        WorkSite work = claimJobSite(arena, site, "minecraft:lectern");
        MobActor &villager = *work.mVillager;
        require(villager.hasComponentGroup("librarian"), "claiming a lectern did not make a librarian");

        WorkGoal goal(workSettings());
        require(waitForStart(arena, goal, villager), "a villager with a job site did not go to work");
        goal.start(arena.mOwner, villager);

        // the site is reached within 2 blocks of its centre
        const int64_t now = arena.mOwner.getCurrentTick();
        require(work.mVillage->countRecentWorkers(now) == 0, "the village counted work before arrival");
        villager.setPosition(Vector3f(24.5f, 64.0f, 25.5f));
        goal.tick(arena.mOwner, villager);
        require(work.mVillage->countRecentWorkers(now) == 1, "working at the site did not count for the village");
        require(villager.hasComponentGroup("trade_resupply_component_group"), "arriving did not resupply the trades");

        for (int32_t tick = 1; tick <= 250; ++tick) {
            require(goal.canContinueToUse(arena.mOwner, villager), "the shift ended before its active time");
            goal.tick(arena.mOwner, villager);
        }
        require(goal.getWorkedTicks() == 251, "the worked ticks did not follow the shift");
        require(!goal.canContinueToUse(arena.mOwner, villager), "the shift outlasted its active time");
        goal.stop(arena.mOwner, villager);

        // the cooldown holds the goal for 200 calls, then the search interval runs
        for (int32_t call = 0; call < 199; ++call)
            require(!goal.canUse(arena.mOwner, villager), "the goal restarted during its cooldown");
        require(work.mPoi->mFailedTrips == 0, "an arrived trip counted as failed");
    }

    void workStopsInTheRain(Arena &arena) {
        arena.floor(-30, -20, 63, -30, -20);
        const Vector3i site(-24, 64, -24);
        WorkSite work = claimJobSite(arena, site, "minecraft:smithing_table");
        MobActor &villager = *work.mVillager;

        // the rain is looked for at the site's own block, which a solid job block covers: clear it to bare the spot
        arena.mLevel->setBlockState(site.x, site.y, site.z, BlockState("minecraft:air"));
        arena.mLevel->setRainingState(true);
        for (int32_t tick = 0; tick < 30; ++tick)
            arena.mLevel->tickWeatherLevels();

        WorkGoal goal(workSettings());
        require(!waitForStart(arena, goal, villager), "a villager went to work in the rain");

        // a roof keeps the rain off the site
        arena.mLevel->setBlockState(site.x, site.y + 3, site.z, BlockState("minecraft:stone"));
        require(waitForStart(arena, goal, villager), "a sheltered job site was not worked");
        goal.start(arena.mOwner, villager);
        villager.setPosition(Vector3f(-23.5f, 64.0f, -22.5f));
        goal.tick(arena.mOwner, villager);
        require(goal.canContinueToUse(arena.mOwner, villager), "the shelter did not keep the shift going");

        arena.mLevel->setBlockState(site.x, site.y + 3, site.z, BlockState("minecraft:air"));
        require(!goal.canContinueToUse(arena.mOwner, villager), "rain on the site did not stop the shift");
        goal.stop(arena.mOwner, villager);

        arena.mLevel->setRainingState(false);
        for (int32_t tick = 0; tick < 100; ++tick)
            arena.mLevel->tickWeatherLevels();
    }

    void unreachableSiteGivesUp(Arena &arena) {
        arena.floor(40, 50, 63, -10, 0);
        const Vector3i site(45, 64, -5);
        WorkSite work = claimJobSite(arena, site, "minecraft:barrel");
        MobActor &villager = *work.mVillager;
        villager.setPosition(Vector3f(45.5f, 64.0f, 5.5f));

        WorkGoal goal(workSettings());
        require(waitForStart(arena, goal, villager), "the fisherman did not set off");
        goal.start(arena.mOwner, villager);
        // never moving: the repath after 100 ticks finds the mob on its starting block
        for (int32_t tick = 0; tick < 100; ++tick)
            goal.tick(arena.mOwner, villager);
        require(!goal.canContinueToUse(arena.mOwner, villager), "a stuck villager kept walking");
        goal.stop(arena.mOwner, villager);
        require(work.mPoi->mFailedTrips == 1, "the stuck trip did not count as failed");
        require(work.mVillage->countRecentWorkers(arena.mOwner.getCurrentTick()) == 0,
                "a villager that never arrived counted as working");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-villager-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        scheduleFollowsTheClock(arena);
        workCountsForTheVillage(arena);
        workStopsInTheRain(arena);
        unreachableSiteGivesUp(arena);
        std::cout << "villager checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
