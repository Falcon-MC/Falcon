#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const char *const VINDICATOR = "minecraft:vindicator";
    const char *const SPAWNED = "minecraft:entity_spawned";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Vindicators", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -12; x <= 12; x++) {
                for (int32_t z = -12; z <= 12; z++)
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

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event = SPAWNED) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void ticks(int32_t count) {
            for (int32_t tick = 0; tick < count; tick++) {
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                PathFinder::tryReserveSearch(-1);
                mOwner.tickActors();
            }
        }

        // a creative player names the mob with a name tag, as the game does it; mobs never target it
        void name(MobActor &mob, const std::string &name) {
            ServerPlayer &namer = player(9000, Vector3f(-10.5f, 64.0f, -10.5f));
            namer.setGameType((int32_t) GameType::Creative);
            ItemStack tag = mOwner.createItemStack("minecraft:name_tag", 1);
            Tag display = Tag::ofCompound();
            display.putString("Name", name);
            tag.mTag = Tag::ofCompound();
            tag.mTag.put("display", display);
            namer.getInventory().setItemInHand(tag);
            require(mob.onInteract(mOwner, namer) && mob.getNameTag() == name, "the name tag did not name the mob");
        }

        const Actor *targetAfter(MobActor &mob, int32_t ticks) {
            for (int32_t tick = 0; tick < ticks && mob.getTarget(mOwner) == nullptr; tick++)
                this->ticks(1);
            return mob.getTarget(mOwner);
        }
    };

    // players, golems, traders and adult villagers; never a baby villager or another mob
    void defaultTargets() {
        {
            Arena arena;
            MobActor &vindicator = arena.spawn(VINDICATOR, Vector3f(0.5f, 64.0f, 0.5f));
            arena.spawn("minecraft:pig", Vector3f(3.5f, 64.0f, 0.5f));
            MobActor &baby = arena.spawn("minecraft:villager_v2", Vector3f(-3.5f, 64.0f, 0.5f));
            baby.removeComponentGroup("adult");
            baby.addComponentGroup("baby");
            require(arena.targetAfter(vindicator, 60) == nullptr, "the vindicator picked a pig or a baby villager");
        }
        {
            Arena arena;
            MobActor &vindicator = arena.spawn(VINDICATOR, Vector3f(0.5f, 64.0f, 0.5f));
            MobActor &trader = arena.spawn("minecraft:wandering_trader", Vector3f(4.5f, 64.0f, 0.5f));
            require(arena.targetAfter(vindicator, 60) == &trader, "the vindicator ignored a wandering trader");
            // on_target_acquired: the axe goes up
            arena.ticks(1);
            require(vindicator.hasComponentGroup("minecraft:vindicator_aggro")
                            && vindicator.getFlags().get(ActorFlag::Angry),
                    "a vindicator with a target is not aggressive");
        }
    }

    // named Johnny: every mob but illagers; named anything else: back to the usual targets
    void johnny() {
        Arena arena;
        MobActor &vindicator = arena.spawn(VINDICATOR, Vector3f(0.5f, 64.0f, 0.5f));
        arena.spawn("minecraft:pillager", Vector3f(-3.5f, 64.0f, 0.5f));
        arena.name(vindicator, "Johnny");
        require(vindicator.hasComponentGroup("minecraft:vindicator_johnny"), "Johnny did not start");
        require(arena.targetAfter(vindicator, 60) == nullptr, "Johnny went for an illager");

        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(4.5f, 64.0f, 0.5f));
        require(arena.targetAfter(vindicator, 60) == &pig, "Johnny ignored a pig");

        arena.name(vindicator, "Bob");
        require(!vindicator.hasComponentGroup("minecraft:vindicator_johnny")
                        && vindicator.hasComponentGroup("minecraft:default_targeting"),
                "another name did not stop Johnny");
    }

    // raid vindicators break doors from normal difficulty
    void raidDoors(const std::filesystem::path &directory) {
        Arena arena;
        const std::filesystem::path properties = directory / "vindicator.properties";
        std::ofstream(properties) << "difficulty=normal\n";
        arena.mOwner.setProperties(PropertiesSettings(properties.string()));
        MobActor &raider = arena.spawn(VINDICATOR, Vector3f(0.5f, 64.0f, 0.5f), "minecraft:spawn_for_raid");
        MobActor &wanderer = arena.spawn(VINDICATOR, Vector3f(4.5f, 64.0f, 0.5f));
        arena.ticks(1);
        require(raider.getComponent("minecraft:annotation.break_door") != nullptr
                        && raider.getNavigation().getOptions().mCanBreakDoors,
                "a raid vindicator cannot break doors");
        require(!wanderer.getNavigation().getOptions().mCanBreakDoors, "a wandering vindicator breaks doors");
        require(raider.getComponent("minecraft:persistent") != nullptr, "a raid vindicator can despawn");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-vindicator-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        defaultTargets();
        johnny();
        raidDoors(directory);
        std::cout << "vindicator checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Vindicator regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
