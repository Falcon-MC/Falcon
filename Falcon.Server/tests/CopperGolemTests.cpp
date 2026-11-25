#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/PlaceBlockGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/Neutral/GolemBuilding.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/CopperGolemStatueBlockActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const char *const COPPER_GOLEM = "minecraft:copper_golem";
    const char *const OXIDATION = "minecraft:oxidation_level";
    const char *const WAXED = "minecraft:is_waxed";
    const char *const OXIDIZING = "minecraft:copper_oxidizing";
    const char *const BECOMING_STATUE = "minecraft:becoming_statue";
    // the oxidation timer runs 25200 to 27600 seconds
    const int64_t TIMER_MIN_TICKS = 25200 * 20;
    const int64_t TIMER_MAX_TICKS = 27600 * 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Copper golem", "", 1};
        Level *mLevel = nullptr;

        Arena() {
            mLevel = &mOwner.getWorlds().getDefault().getOverworld();
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const Vector3f &position, const Tag *saved = nullptr) {
            ServerActor *actor = mOwner.spawnActor(*mLevel, COPPER_GOLEM, position, [saved](ServerActor &spawned) {
                if (saved != nullptr)
                    spawned.loadNbt(*saved);
            });
            MobActor *mob = dynamic_cast<MobActor *>(actor);
            require(mob != nullptr, "the copper golem did not spawn");
            mob->setPosition(position);
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

        void advance(int64_t ticks) {
            for (int64_t tick = 0; tick < ticks; ++tick)
                mLevel->tickTime();
        }
    };

    int32_t stageOf(const MobActor &golem) {
        return golem.getIntProperty(OXIDATION, -1);
    }

    bool waxedOf(const MobActor &golem) {
        return golem.getIntProperty(WAXED, 0) != 0;
    }

    MobActor *golemAt(Arena &arena, const Vector3f &position) {
        for (auto &entry: arena.mOwner.getActors()) {
            const Vector3f &at = entry.second->getPosition();
            if (entry.second->getTypeId() == COPPER_GOLEM && !entry.second->isDead() && at.x == position.x
                && at.y == position.y && at.z == position.z)
                return dynamic_cast<MobActor *>(entry.second.get());
        }
        return nullptr;
    }

    void buildAtEveryStage(Arena &arena) {
        Level &level = *arena.mLevel;
        const char *const bodies[] = {"minecraft:copper_block",       "minecraft:exposed_copper",
                                      "minecraft:weathered_copper",   "minecraft:oxidized_copper",
                                      "minecraft:waxed_copper",       "minecraft:waxed_exposed_copper",
                                      "minecraft:waxed_weathered_copper", "minecraft:waxed_oxidized_copper"};
        const char *const chests[] = {"minecraft:copper_chest", "minecraft:exposed_copper_chest",
                                      "minecraft:weathered_copper_chest", "minecraft:oxidized_copper_chest"};
        const char *const events[] = {"minecraft:from_player_default", "minecraft:from_player_exposed",
                                      "minecraft:from_player_weathered", "minecraft:from_player_oxidized"};

        for (int32_t index = 0; index < 8; ++index) {
            const int32_t stage = index % 4;
            const int32_t x = -20 + index * 3;
            const int32_t y = 100;
            const int32_t z = -20;
            BlockState head("minecraft:carved_pumpkin");
            head.mStates.putString("minecraft:cardinal_direction", "east");
            level.setBlockState(x, y, z, BlockState(bodies[index]));
            level.setBlockState(x, y + 1, z, head);
            require(GolemBuilding::find(level, Vector3i(x, y + 1, z)).mKind == GolemBuilding::Kind::Copper,
                    std::string("no copper golem pattern on ") + bodies[index]);
            require(GolemBuilding::tryBuild(arena.mOwner, level, Vector3i(x, y + 1, z)),
                    std::string("no copper golem built on ") + bodies[index]);

            const BlockState chest = level.getBlockState(x, y, z);
            require(chest.mName == chests[stage], std::string("the body did not turn into ") + chests[stage]);
            require(chest.mStates.getString("minecraft:cardinal_direction", "") == "east",
                    "the chest does not face the way the head did");
            require(level.getBlockState(x, y + 1, z).mName == "minecraft:air", "the head stayed");

            MobActor *golem = golemAt(arena, Vector3f((float) x + 0.5f, (float) (y + 1), (float) z + 0.5f));
            require(golem != nullptr, "the golem does not stand in the head's cell");
            require(golem->getSpawnEvent() == events[stage], std::string("the golem was not spawned with ") + events[stage]);
            require(golem->getRotation().y == -90.0f, "the golem does not face east");
            golem->startDefinition(arena.mOwner);
            require(stageOf(*golem) == stage, "the golem's stage is not the block's");
            require(!waxedOf(*golem), "a golem built on waxed copper came out waxed");
            require(golem->hasComponentGroup(stage == 3 ? BECOMING_STATUE : OXIDIZING),
                    "the golem did not start its oxidation");
        }

        // stone under the pumpkin is no golem
        level.setBlockState(10, 100, 10, BlockState("minecraft:stone"));
        level.setBlockState(10, 101, 10, BlockState("minecraft:carved_pumpkin"));
        require(GolemBuilding::find(level, Vector3i(10, 101, 10)).mKind == GolemBuilding::Kind::None,
                "a pumpkin on stone matched a golem");
    }

    void oxidizeIntoStatue(Arena &arena) {
        Level &level = *arena.mLevel;
        level.setBlockState(0, 99, 5, BlockState("minecraft:stone"));
        MobActor &golem = arena.spawn(Vector3f(0.5f, 100.0f, 5.5f));
        golem.setRotation(Vector3f(0.0f, 0.0f, 0.0f));
        golem.startDefinition(arena.mOwner);
        require(stageOf(golem) == 0 && golem.hasComponentGroup(OXIDIZING), "a spawned golem does not oxidize");

        golem.tick(arena.mOwner);
        arena.advance(TIMER_MIN_TICKS - 1);
        golem.tick(arena.mOwner);
        require(stageOf(golem) == 0, "the golem oxidized before 25200 seconds");
        arena.advance(TIMER_MAX_TICKS - TIMER_MIN_TICKS + 1);
        golem.tick(arena.mOwner);
        require(stageOf(golem) == 1, "the golem was not exposed after 27600 seconds");

        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        require(stageOf(golem) == 2, "the golem did not weather");
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        require(stageOf(golem) == 3, "the golem did not oxidize");
        require(golem.hasComponentGroup(BECOMING_STATUE) && !golem.hasComponentGroup(OXIDIZING),
                "an oxidized golem kept its timer or is not becoming a statue");

        const json::Value *component = golem.getComponent("minecraft:behavior.place_block");
        require(component != nullptr, "a golem becoming a statue has no place_block goal");
        std::unique_ptr<Goal> goal = PlaceBlockGoal::create(*component);
        require(goal != nullptr, "the statue goal was not built");
        bool placed = false;
        // a 0.0058 chance a tick
        for (int32_t attempt = 0; attempt < 20000 && !placed; ++attempt)
            placed = goal->canUse(arena.mOwner, golem);
        require(placed, "the golem never turned into a statue");
        goal->start(arena.mOwner, golem);

        const BlockState statue = level.getBlockState(0, 100, 5);
        require(statue.mName == "minecraft:oxidized_copper_golem_statue", "no oxidized statue was placed");
        require(statue.mStates.getString("minecraft:cardinal_direction", "") == "south",
                "the statue does not face the way the golem did");
        const CopperGolemStatueBlockActor *stored =
                level.getBlockActors().find<CopperGolemStatueBlockActor>(Vector3i(0, 100, 5));
        require(stored != nullptr && stored->hasStoredActor(), "the statue did not keep the golem");
        require(golem.hasComponentGroup("minecraft:became_statue"), "the golem was not removed into its statue");
    }

    void waxAndScrape(Arena &arena) {
        MobActor &golem = arena.spawn(Vector3f(8.5f, 100.0f, 8.5f));
        golem.startDefinition(arena.mOwner);
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        require(stageOf(golem) == 1, "the golem did not reach the exposed stage");
        ServerPlayer &player = arena.player(9401, Vector3f(9.5f, 100.0f, 8.5f));
        PlayerInventory &inventory = player.getInventory();

        inventory.setItemInHand(arena.mOwner.createItemStack("minecraft:honeycomb", 2));
        require(golem.onInteract(arena.mOwner, player), "the honeycomb did not wax the golem");
        require(waxedOf(golem) && !golem.hasComponentGroup(OXIDIZING), "a waxed golem still oxidizes");
        require(inventory.getItemInHand().mCount == 1, "waxing did not use the honeycomb");
        require(!golem.onInteract(arena.mOwner, player), "a waxed golem took more wax");

        inventory.setItemInHand(arena.mOwner.createItemStack("minecraft:iron_axe", 1));
        require(golem.onInteract(arena.mOwner, player), "the axe did not take the wax off");
        require(!waxedOf(golem) && stageOf(golem) == 1, "the wax came off with a stage");
        require(golem.hasComponentGroup(OXIDIZING), "the unwaxed golem did not oxidize again");
        require(inventory.getItemInHand().mDamage == 1, "taking the wax off did not wear the axe");

        require(golem.onInteract(arena.mOwner, player), "the axe did not scrape the golem");
        require(stageOf(golem) == 0 && golem.hasComponentGroup(OXIDIZING), "scraping did not lower the stage");
        require(inventory.getItemInHand().mDamage == 2, "scraping did not wear the axe");
        require(!golem.onInteract(arena.mOwner, player), "an unoxidized golem was scraped");

        // scraping starts the timer over
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        golem.tick(arena.mOwner);
        arena.advance(TIMER_MAX_TICKS - 100);
        golem.fireEvent(arena.mOwner, "minecraft:remove_oxidation_layer");
        golem.tick(arena.mOwner);
        arena.advance(200);
        golem.tick(arena.mOwner);
        require(stageOf(golem) == 0, "scraping did not restart the oxidation timer");

        // lightning takes a layer and deals no damage
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        const float health = golem.getHealth();
        arena.mOwner.strikeLightning(*arena.mLevel, golem.getPosition());
        require(stageOf(golem) == 1, "lightning did not take an oxidation layer");
        require(golem.getHealth() == health, "lightning hurt the golem");
    }

    void persistence(Arena &arena) {
        MobActor &golem = arena.spawn(Vector3f(-8.5f, 100.0f, 8.5f));
        golem.startDefinition(arena.mOwner);
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        golem.fireEvent(arena.mOwner, "minecraft:oxidize_copper");
        golem.getEquipment().setSlot(MobEquipment::MAINHAND, arena.mOwner.createItemStack("minecraft:diamond", 5));
        golem.tick(arena.mOwner);
        const Tag saved = golem.saveNbt();
        require(saved.contains("TimeStamp") && saved.getByte("HasExecuted", 1) == 0, "the timer was not saved");

        MobActor &loaded = arena.spawn(Vector3f(-12.5f, 100.0f, 8.5f), &saved);
        loaded.getEquipment().loadNbt(saved, arena.mOwner.getCodecContext());
        require(stageOf(loaded) == 2 && !waxedOf(loaded), "the stage did not survive a reload");
        require(loaded.hasComponentGroup(OXIDIZING), "the oxidation did not survive a reload");
        const ItemStack &carried = loaded.getEquipment().getSlot(MobEquipment::MAINHAND);
        require(!carried.isAir() && carried.mCount == 5, "the carried item did not survive a reload");

        // the reloaded timer runs out when the saved one would have, not after a new roll
        const int64_t stamp = saved.getLong("TimeStamp", 0);
        const int64_t now = arena.mLevel->getWorldTick();
        loaded.tick(arena.mOwner);
        arena.advance(stamp - now - 1);
        loaded.tick(arena.mOwner);
        require(stageOf(loaded) == 2, "the reloaded timer ran out early");
        arena.advance(1);
        loaded.tick(arena.mOwner);
        require(stageOf(loaded) == 3, "the reloaded timer did not run out on its saved tick");

        loaded.fireEvent(arena.mOwner, "minecraft:wax_on");
        const Tag waxed = loaded.saveNbt();
        MobActor &again = arena.spawn(Vector3f(-16.5f, 100.0f, 8.5f), &waxed);
        require(waxedOf(again) && stageOf(again) == 3, "the wax did not survive a reload");
        require(!again.hasComponentGroup(OXIDIZING) && !again.hasComponentGroup(BECOMING_STATUE),
                "a reloaded waxed golem oxidizes");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-copper-golem-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena;
        buildAtEveryStage(arena);
        oxidizeIntoStatue(arena);
        waxAndScrape(arena);
        persistence(arena);
        std::cout << "copper golem checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
