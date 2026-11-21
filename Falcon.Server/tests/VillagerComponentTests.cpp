#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Navigation/NodeEvaluator.h"
#include "World/Actor/AI/Navigation/PathOptions.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Movement/BandwidthOptimization.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
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
        ServerNetworkHandler mOwner{"VillagerComponents", "", 1};
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

        MobActor &adult(const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, "minecraft:villager_v2", position));
            require(mob != nullptr, "the villager did not spawn");
            mob->startDefinition(mOwner);
            for (const char *group: SCHEDULE_GROUPS)
                mob->removeComponentGroup(group);
            mob->addComponentGroup("adult");
            mob->addComponentGroup("unskilled");
            mob->tick(mOwner);
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        int32_t countVillagers() {
            int32_t count = 0;
            for (const auto &entry: mOwner.getActors()) {
                if (entry.second->isAlive() && std::string(entry.second->getTypeId()) == "minecraft:villager_v2")
                    ++count;
            }
            return count;
        }
    };

    void villagersPreferPaths(Arena &arena) {
        MobActor &villager = arena.adult(Vector3f(0.5f, 64.0f, 0.5f));
        const PathOptions options = PathOptions::read(villager);
        require(options.mHasPreferredPath, "the adult has no preferred path");
        require(options.mDefaultBlockCost == 3.0f && options.mJumpCost == 20.0f, "the adult path costs were not read");

        arena.mLevel->setBlockState(2, 63, 0, BlockState("minecraft:grass_block"));
        arena.mLevel->setBlockState(3, 63, 0, BlockState("minecraft:cobblestone"));
        NodeEvaluator evaluator;
        evaluator.prepare(*arena.mLevel, villager, 0.6f, 1.9f, villager.getPosition(), false, options);
        require(evaluator.extraCost(2, 64, 0) == 30, "an unlisted block did not cost the default");
        require(evaluator.extraCost(3, 64, 0) == 10, "a listed block did not cost its own");
        require(evaluator.jumpCost() == 200, "a jump did not cost its own");

        MobActor &plain = *dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(*arena.mLevel, "minecraft:pig", Vector3f(5.5f, 64.0f, 5.5f)));
        const PathOptions pigOptions = PathOptions::read(plain);
        NodeEvaluator pigEvaluator;
        pigEvaluator.prepare(*arena.mLevel, plain, 0.9f, 0.9f, plain.getPosition(), false, pigOptions);
        require(pigEvaluator.extraCost(2, 64, 0) == 0 && pigEvaluator.jumpCost() == 0,
                "a mob without preferred_path paid for blocks");
    }

    void pistonsPushVillagers(Arena &arena) {
        const Vector3i piston(-5, 64, -5);
        BlockState state("minecraft:piston");
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates("minecraft:piston");
        require(defaults != nullptr, "the piston has no default state");
        state.mStates = *defaults;
        state.mStates.putInt("facing_direction", 1);
        arena.mLevel->setBlockState(piston.x, piston.y, piston.z, state);
        require(PistonSystem::getPistonFace(state) == RedstoneFace::UP, "facing_direction 1 is not up");

        MobActor &villager = arena.adult(Vector3f(-4.5f, 65.0f, -4.5f));
        arena.mLevel->setBlockState(piston.x + 1, piston.y, piston.z, BlockState("minecraft:redstone_block"));
        PistonSystem::onRedstoneUpdate(arena.mOwner, *arena.mLevel, piston, state);
        require(villager.getPosition().y == 66.0f, "the extending piston did not push the villager up");
    }

    void eggHatchesABaby(Arena &arena) {
        MobActor &villager = arena.adult(Vector3f(5.5f, 64.0f, -5.5f));
        ServerPlayer &player = arena.player(77, Vector3f(5.5f, 64.0f, -3.5f));
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:pig_spawn_egg", 2));
        const int32_t before = arena.countVillagers();
        villager.onInteract(arena.mOwner, player);
        require(arena.countVillagers() == before, "another mob's egg hatched a villager");

        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:villager_spawn_egg", 2));
        require(villager.onInteract(arena.mOwner, player), "a villager egg used on a villager did nothing");
        require(arena.countVillagers() == before + 1, "the egg hatched no baby");
        require(player.getInventory().getItemInHand().mCount == 1, "the egg was not used up");
    }
}

namespace {
    // three quarters of max_dropped_ticks at the edge, scaled down by the distance inside it
    void bandwidthGapGrowsWithDistance() {
        BandwidthOptimization::Values values;
        values.mMaxOptimizedDistance = 80.0f;
        values.mMaxDroppedTicks = 10;
        require(BandwidthOptimization::allowedGap(values, 0.0f) == 0, "a viewer on the actor got a gap");
        require(BandwidthOptimization::allowedGap(values, 40.0f) == 3, "half the distance did not give 3 ticks");
        require(BandwidthOptimization::allowedGap(values, 200.0f) == 7, "past the distance did not give 7 ticks");
        values.mMaxOptimizedDistance = 0.0f;
        values.mMaxDroppedTicks = 0;
        require(BandwidthOptimization::allowedGap(values, 30.0f) == 0, "a zeroed entry still dropped ticks");
    }

    // minecraft:persistent keeps a mob even when it came from the natural spawner
    void persistentComponentKeepsTheMob(Arena &arena) {
        MobActor &villager = arena.adult(Vector3f(0.5f, 64.0f, 0.5f));
        villager.setPersistent(false);
        require(villager.isPersistent(), "a villager without the persistent flag lost minecraft:persistent");

        MobActor *cow = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(*arena.mLevel, "minecraft:cow", Vector3f(3.5f, 64.0f, 3.5f)));
        require(cow != nullptr, "the cow did not spawn");
        cow->startDefinition(arena.mOwner);
        cow->setPersistent(false);
        require(!cow->isPersistent(), "a cow without minecraft:persistent stayed persistent");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-villager-components-"
               + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        BlockPaletteRegistry::getInstance().initialize();
        Arena arena(directory);
        villagersPreferPaths(arena);
        pistonsPushVillagers(arena);
        eggHatchesABaby(arena);
        persistentComponentKeepsTheMob(arena);
        bandwidthGapGrowsWithDistance();
        std::cout << "villager component checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
