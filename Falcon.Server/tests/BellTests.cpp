#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Misc/ItemActor.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/HideGoal.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Actor/BellBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/BellBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Village/PoiBlueprint.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Village.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the spawn chunk keeps its generated terrain: the cases stay in the empty chunks east of it
    struct Arena {
        ServerNetworkHandler mOwner{"Bells", "", 1};
        Level *mLevel = nullptr;
        ServerPlayer *mPlayer = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            mLevel->setOwner(&mOwner);
            for (int32_t x = 1; x <= 9; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                    mLevel->getBlockActors().loadChunk(x, z, {}, mOwner.getCodecContext());
                }
            }
            const NetworkIdentifier id("bell", 9001);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, 9001).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(20.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.setGameType((int32_t) GameType::Survival);
            mPlayer = &player;
        }

        // a bell standing on stone, facing the given direction
        Vector3i bell(int32_t x, const char *attachment, int32_t direction) {
            const Vector3i at(x, 100, 0);
            mLevel->setBlock(Vector3i(x, 99, 0), BlockState("minecraft:stone"), false);
            BlockState state = VanillaBlocks::fromIdentifier("minecraft:bell")->toBlockState();
            state.mStates.putString("attachment", attachment);
            state.mStates.putInt("direction", direction);
            mLevel->setBlock(at, state, false);
            return at;
        }

        BlockState at(const Vector3i &position) {
            return mLevel->getBlockState(position.x, position.y, position.z);
        }

        const BellBlock &block(const Vector3i &position) {
            return *dynamic_cast<const BellBlock *>(VanillaBlocks::fromState(at(position)));
        }

        BellBlockActor *actor(const Vector3i &position) {
            return mLevel->getBlockActors().find<BellBlockActor>(position);
        }

        bool ringing(const Vector3i &position) {
            BellBlockActor *bell = actor(position);
            return bell != nullptr && bell->isRinging();
        }

        // a yaw of 0 faces south, along z
        bool hit(const Vector3i &position, float yaw) {
            mPlayer->setRotation(Vector3f(0.0f, yaw, yaw));
            const BlockState state = at(position);
            return block(position).onInteract(mOwner, *mPlayer, position, state);
        }

        void power(const Vector3i &position, bool on) {
            mLevel->setBlock(Vector3i(position.x + 1, position.y, position.z),
                             BlockState(on ? "minecraft:redstone_block" : "minecraft:air"), false);
            const BlockState state = at(position);
            block(position).onRedstonePower(mOwner, *mLevel, position, state);
        }

        void tick(int32_t ticks) {
            for (int32_t tick = 0; tick < ticks; ++tick)
                mLevel->getBlockActors().tick(mOwner);
        }

        MobActor &villager(const Vector3f &position) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, "minecraft:villager_v2", position));
            require(mob != nullptr, "the villager did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }
    };

    // a player rings a standing bell along its axis, a wall bell across it, a hanging one always
    void hitting(Arena &arena) {
        const Vector3i standing = arena.bell(20, "standing", 0);
        require(!arena.hit(standing, 90.0f) && !arena.ringing(standing), "a standing bell rang across its axis");
        require(arena.hit(standing, 0.0f) && arena.ringing(standing), "a standing bell did not ring along its axis");
        require(arena.actor(standing)->getDirection() == 2, "facing south did not swing the bell towards 2");
        require(arena.at(standing).mStates.getByte("toggle_bit", 0) == 1, "ringing did not flip the toggle bit");

        const Vector3i wall = arena.bell(22, "side", 0);
        require(!arena.hit(wall, 0.0f), "a wall bell rang along its axis");
        require(arena.hit(wall, 90.0f) && arena.ringing(wall), "a wall bell did not ring across its axis");

        const Vector3i hanging = arena.bell(24, "hanging", 1);
        require(arena.hit(hanging, 0.0f) && arena.hit(hanging, 90.0f), "a hanging bell did not always ring");
    }

    // a ring swings for 25 ticks
    void swinging(Arena &arena) {
        const Vector3i bell = arena.bell(26, "hanging", 0);
        arena.hit(bell, 0.0f);
        arena.tick(24);
        require(arena.ringing(bell), "the bell stopped before 25 ticks");
        arena.tick(1);
        require(!arena.ringing(bell), "the bell still rang after 25 ticks");
    }

    // power coming on rings it the way it faces; staying on or going off does not
    void redstone(Arena &arena) {
        const Vector3i bell = arena.bell(28, "standing", 3);
        arena.power(bell, true);
        require(arena.ringing(bell), "power did not ring the bell");
        require(arena.actor(bell)->getDirection() == 3, "power did not swing it the way it faces");
        arena.tick(25);
        arena.power(bell, true);
        require(!arena.ringing(bell), "power staying on rang the bell again");
        arena.power(bell, false);
        require(!arena.ringing(bell), "power going off rang the bell");
        arena.power(bell, true);
        require(arena.ringing(bell), "power coming back did not ring the bell");
        // placed beside a live source, it rings at once
        const Vector3i placed = arena.bell(36, "standing", 0);
        arena.mLevel->setBlock(Vector3i(37, 100, 0), BlockState("minecraft:redstone_block"), false);
        const BlockState state = arena.at(placed);
        arena.block(placed).onPlaced(arena.mOwner, *arena.mPlayer, placed, state, ItemStack::air(), 1);
        require(arena.ringing(placed), "a bell placed by power did not ring");
    }

    // an arrow rings it the way it faces and flies on; a wind charge leaves that to its burst
    void projectiles(Arena &arena) {
        const Vector3i bell = arena.bell(30, "hanging", 1);
        ServerActor *arrow = arena.mOwner.spawnActor(*arena.mLevel, "minecraft:arrow", Vector3f(30.5f, 102.0f, 0.5f));
        require(arrow != nullptr, "no arrow spawned");
        const BlockState state = arena.at(bell);
        require(!arena.block(bell).onProjectileHit(arena.mOwner, *arena.mLevel, bell, state, *arrow),
                "an arrow stopped on the bell");
        require(arena.ringing(bell) && arena.actor(bell)->getDirection() == 1, "an arrow did not ring the bell");

        const Vector3i other = arena.bell(32, "hanging", 0);
        ServerActor *charge = arena.mOwner.spawnActor(*arena.mLevel, "minecraft:wind_charge_projectile",
                                                      Vector3f(32.5f, 102.0f, 0.5f));
        require(charge != nullptr, "no wind charge spawned");
        arena.block(other).onProjectileHit(arena.mOwner, *arena.mLevel, other, arena.at(other), *charge);
        require(!arena.ringing(other), "a wind charge's hit rang the bell");
    }

    // a thrown item bounces off it, away from its middle, and rings it the way it faces; a sliding one does not
    void thrownItems(Arena &arena) {
        // away from the redstone block the placing case left at 37
        const Vector3i bell = arena.bell(44, "hanging", 3);
        ItemActor *item = ItemActorHandler::dropItem(arena.mOwner, *arena.mLevel, Vector3f(43.9f, 100.5f, 0.5f),
                                                     arena.mOwner.createItemStack("minecraft:stone", 1),
                                                     Vector3f(0.3f, 0.0f, 0.05f), 10);
        require(item != nullptr, "no item dropped");
        require(!arena.block(bell).onItemHit(arena.mOwner, *arena.mLevel, bell, arena.at(bell), *item)
                        && !arena.ringing(bell),
                "an item moving on two axes rang the bell");

        item->setMotion(Vector3f(0.3f, -0.1f, 0.05f));
        require(arena.block(bell).onItemHit(arena.mOwner, *arena.mLevel, bell, arena.at(bell), *item),
                "a thrown item did not bounce off the bell");
        require(arena.ringing(bell) && arena.actor(bell)->getDirection() == 3, "a thrown item did not ring the bell");
        const Vector3f &bounced = item->getMotion();
        require(std::fabs(bounced.x + 0.3f) < 0.001f && std::fabs(bounced.y + 0.1f) < 0.001f
                        && std::fabs(bounced.z - 0.05f) < 0.001f,
                "the item did not bounce away from the bell's middle");

        // the item's own fall runs into the bell
        const Vector3i other = arena.bell(48, "standing", 1);
        ItemActor *thrown = ItemActorHandler::dropItem(arena.mOwner, *arena.mLevel, Vector3f(47.7f, 100.3f, 0.5f),
                                                       arena.mOwner.createItemStack("minecraft:stone", 1),
                                                       Vector3f(0.5f, 0.2f, 0.05f), 10);
        require(thrown != nullptr, "no item thrown");
        ItemActorHandler::tickItemActors(arena.mOwner);
        require(arena.ringing(other), "an item flying into a bell did not ring it");
        require(thrown->getMotion().x < 0.0f, "an item flying into a bell did not bounce back");
    }

    // a burst rings it the way it faces, but not while it still swings
    void windBurst(Arena &arena) {
        const Vector3i bell = arena.bell(34, "hanging", 2);
        arena.block(bell).onWindBurst(arena.mOwner, *arena.mLevel, bell, arena.at(bell));
        require(arena.ringing(bell) && arena.actor(bell)->getDirection() == 2, "a burst did not ring the bell");
        const int8_t toggled = arena.at(bell).mStates.getByte("toggle_bit", 0);
        arena.block(bell).onWindBurst(arena.mOwner, *arena.mLevel, bell, arena.at(bell));
        require(arena.at(bell).mStates.getByte("toggle_bit", 0) == toggled, "a burst rang a swinging bell");
    }

    // villagers within 32 blocks across and 12 up or down hear it; then the bell rests 300 ticks
    void alert(Arena &arena) {
        const Vector3i bell = arena.bell(80, "hanging", 0);
        MobActor &near = arena.villager(Vector3f(110.5f, 100.0f, 0.5f));
        MobActor &far = arena.villager(Vector3f(116.5f, 100.0f, 0.5f));
        MobActor &below = arena.villager(Vector3f(80.5f, 86.0f, 0.5f));
        auto *pig = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(*arena.mLevel, "minecraft:pig", Vector3f(84.5f, 100.0f, 0.5f)));
        require(pig != nullptr, "the pig did not spawn");
        arena.hit(bell, 0.0f);
        require(!pig->heardBell(), "a pig, which can't hide, heard the bell");
        require(near.heardBell(), "a villager 30 blocks off did not hear the bell");
        require(!far.heardBell(), "a villager 36 blocks off heard the bell");
        require(!below.heardBell(), "a villager 14 blocks below heard the bell");

        MobActor &late = arena.villager(Vector3f(82.5f, 100.0f, 0.5f));
        arena.tick(30);
        arena.hit(bell, 0.0f);
        require(!late.heardBell(), "the bell sent mobs hiding again before its rest");
        arena.tick(270);
        arena.hit(bell, 0.0f);
        require(late.heardBell(), "the bell did not send mobs hiding after its rest");
    }

    BlockState bedPart(bool head) {
        BlockState state(BlockNames::BED);
        state.mStates.putInt("direction", 0);
        state.mStates.putByte("head_piece_bit", head ? 1 : 0);
        state.mStates.putByte("occupied_bit", 0);
        return state;
    }

    // with no raid, only a bell sends a villager to hide at its bed
    void toBed(Arena &arena) {
        const Vector3i head(70, 100, 0);
        for (int32_t x = 66; x <= 74; ++x) {
            for (int32_t z = -4; z <= 4; ++z)
                arena.mLevel->setBlock(Vector3i(x, 99, z), BlockState("minecraft:stone"), false);
        }
        arena.mLevel->setBlock(Vector3i(head.x, head.y, head.z - 1), bedPart(false), false);
        arena.mLevel->setBlock(head, bedPart(true), false);
        auto village = std::make_shared<Village>();
        auto bed = std::make_shared<PoiInstance>(head, *PoiBlueprints::find(BlockNames::BED));
        require(village->addPoi(bed), "the bed was refused");
        MobActor &villager = arena.villager(Vector3f(72.5f, 100.0f, 0.5f));
        villager.getDweller().joinVillage(arena.mOwner, villager, *village);
        require(village->getClaimedPoi(villager.getUniqueId(), PoiType::Bed) != nullptr, "the villager has no bed");

        HideGoal goal(0.8f, PoiType::Bed, 600);
        for (int32_t call = 0; call < 125; ++call)
            require(!goal.canUse(arena.mOwner, villager), "a villager hid with no raid and no bell");
        villager.setHeardBell(true);
        bool started = false;
        for (int32_t call = 0; call < 125 && !started; ++call)
            started = goal.canUse(arena.mOwner, villager);
        require(started, "a villager that heard the bell did not head for its bed");
    }

    // the bell calls a villager back four more times, then lets go
    void hiding(Arena &arena) {
        MobActor &villager = arena.villager(Vector3f(60.5f, 100.0f, 0.5f));
        HideGoal goal(0.8f, PoiType::Bed, 600);
        villager.setHeardBell(true);
        for (int32_t walk = 0; walk < HideGoal::BELL_TRIES; ++walk) {
            goal.stop(arena.mOwner, villager);
            require(villager.heardBell(), "the villager forgot the bell after " + std::to_string(walk + 1) + " walks");
        }
        goal.stop(arena.mOwner, villager);
        require(!villager.heardBell(), "the villager kept hiding after five walks");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-bells-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        hitting(arena);
        swinging(arena);
        redstone(arena);
        projectiles(arena);
        windBurst(arena);
        thrownItems(arena);
        alert(arena);
        toBed(arena);
        hiding(arena);
    } catch (const std::exception &error) {
        std::cout << "Bell regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
