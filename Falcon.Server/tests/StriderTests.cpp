#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/MoveToLiquidGoal.h"
#include "World/Actor/AI/Goal/RiseToLiquidLevelGoal.h"
#include "World/Actor/AI/Navigation/Path.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/RideControlSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/RideSystem.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t TICKS_PER_SECOND = 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Striders", "", 1};
        Level &mLevel;

        explicit Arena(DimensionType dimension) : mLevel(mOwner.getWorlds().getDefault().getLevel(dimension)) {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x <= 8; x++) {
                for (int32_t z = -8; z <= 8; z++)
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

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void ticks(int32_t count) {
            // the world tick moves with the overworld, as a server tick does
            for (int32_t tick = 0; tick < count; tick++) {
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                // the server tick stays put without a full tick: a reservation for another tick frees the path budget
                PathFinder::tryReserveSearch(-1);
                mOwner.tickActors();
            }
        }

        size_t countOf(const char *identifier) {
            size_t found = 0;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    ++found;
            }
            return found;
        }

        MobActor *first(const char *identifier) {
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    return dynamic_cast<MobActor *>(entry.second.get());
            }
            return nullptr;
        }
    };

    void lavaPool(Arena &arena, int32_t fromX, int32_t toX, int32_t fromZ, int32_t toZ) {
        for (int32_t x = fromX; x <= toX; x++) {
            for (int32_t z = fromZ; z <= toZ; z++) {
                arena.mLevel.setBlockState(x, 61, z, BlockState("minecraft:stone"));
                arena.mLevel.setBlockState(x, 62, z, BlockState("minecraft:lava"));
                arena.mLevel.setBlockState(x, 63, z, BlockState("minecraft:lava"));
            }
        }
    }

    // rise_to_liquid_level liquid_y_offset 0.25, rise_delta and sink_delta 0.01: the goal aims the feet at the
    // lava surface and stops pushing within 0.4 of it
    void risesToTheSurface() {
        Arena arena(DimensionType::Nether);
        arena.player(1, Vector3f(-6.5f, 64.0f, -6.5f));
        lavaPool(arena, 0, 6, 0, 6);
        const float target = RiseToLiquidLevelGoal::targetHeight(arena.mLevel, Vector3f(3.5f, 62.0f, 3.5f), 0.25f);
        require(target > 63.8f && target < 64.0f, "the settle height over a full lava block is " + std::to_string(target));
        require(RiseToLiquidLevelGoal::targetHeight(arena.mLevel, Vector3f(10.5f, 64.0f, 10.5f), 0.25f) < 0.0f,
                "a dry column has a settle height");

        MobActor &strider = arena.spawn("minecraft:strider", Vector3f(3.5f, 62.0f, 3.5f), "spawn_adult");
        RiseToLiquidLevelGoal goal(0.25f, 0.01f, 0.01f);
        require(goal.canUse(arena.mOwner, strider), "the goal does not run in lava");
        strider.setMotion(Vector3f());
        goal.tick(arena.mOwner, strider);
        require(std::fabs(strider.getMotion().y - 0.01f) < 1.0e-6f, "a strider below the surface is not lifted by 0.01");

        strider.setPosition(Vector3f(3.5f, 63.45f, 3.5f));
        strider.setMotion(Vector3f(0.0f, -0.05f, 0.0f));
        goal.tick(arena.mOwner, strider);
        require(strider.getMotion().y == 0.0f, "a strider within 0.4 of the surface kept its vertical speed");

        // the probe starts half a block over the feet: above that it finds no lava and lowers the mob
        strider.setPosition(Vector3f(3.5f, 63.7f, 3.5f));
        strider.setMotion(Vector3f());
        goal.tick(arena.mOwner, strider);
        require(std::fabs(strider.getMotion().y + 0.01f) < 1.0e-6f, "a strider high in the lava is not lowered by 0.01");

        strider.setPosition(Vector3f(10.5f, 64.0f, 10.5f));
        require(!goal.canUse(arena.mOwner, strider), "the goal runs out of any liquid");
    }

    // navigation can_walk_in_lava without can_sink: no gravity or lava drag in lava, the rise goal lifts the strider
    // to the surface; its paths run and end on top of the lava, and it climbs out onto the bank
    void walksOnDeepLava() {
        Arena arena(DimensionType::Nether);
        for (int32_t x = -8; x <= 8; x++) {
            for (int32_t z = -8; z <= 8; z++) {
                const bool pool = x >= -4 && x <= 4 && z >= -3 && z <= 3;
                for (int32_t y = 57; y <= 63; y++)
                    arena.mLevel.setBlockState(x, y, z,
                                               BlockState(pool && y > 57 ? "minecraft:lava" : "minecraft:stone"));
            }
        }
        ServerPlayer &player = arena.player(1, Vector3f(8.5f, 64.0f, 0.5f));
        MobActor &strider = arena.spawn("minecraft:strider", Vector3f(0.5f, 58.0f, 0.5f), "spawn_adult");
        // the rise goal settles the feet half a block into the top lava block
        int32_t surfaced = -1;
        for (int32_t tick = 0; tick < 10 * TICKS_PER_SECOND && surfaced < 0; tick++) {
            arena.ticks(1);
            const Vector3f position = strider.getPosition();
            const bool overLava = std::fabs(position.x) < 4.0f && std::fabs(position.z) < 3.0f;
            if (overLava && position.y > 63.2f && position.y < 64.0f)
                surfaced = tick;
        }
        require(surfaced >= 0, "a strider in deep lava stayed at " + std::to_string(strider.getPosition().y));

        // from the lava bed to the lava bed: the path starts, runs and ends on top of the lava
        strider.setPosition(Vector3f(-2.5f, 58.0f, 0.5f));
        Path path;
        require(PathFinder::get().findPath(arena.mLevel, strider, Vector3f(3.5f, 58.0f, 0.5f),
                                           strider.getNavigation().getOptions(), path)
                        && path.reachesTarget(),
                "no path across the lava");
        for (const Vector3f &waypoint: path.getWaypoints())
            require(waypoint.y == 64.0f, "a path node sits in the lava at " + std::to_string(waypoint.y));

        strider.setPosition(Vector3f(-0.5f, 63.5f, 0.5f));
        strider.setMotion(Vector3f());
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:warped_fungus", 1));
        arena.ticks(15 * TICKS_PER_SECOND);
        const Vector3f end = strider.getPosition();
        require(end.x > 5.0f && end.y >= 64.0f,
                "the tempted strider did not cross the lava onto the bank: " + std::to_string(end.x) + ", "
                        + std::to_string(end.y));
    }

    void shiversOnLand() {
        Arena arena(DimensionType::Nether);
        MobActor &strider = arena.spawn("minecraft:strider", Vector3f(0.5f, 64.0f, 0.5f), "spawn_adult");
        arena.ticks(2);
        require(strider.hasComponentGroup("minecraft:start_suffocating") && strider.getFlags().get(ActorFlag::Shaking),
                "a strider out of lava does not shiver");
        const Tag data = strider.saveNbt();
        MobActor *loaded = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:strider", Vector3f(4.5f, 64.0f, 0.5f)));
        loaded->loadNbt(data);
        require(loaded->hasComponentGroup("minecraft:start_suffocating"), "the cold state was lost on reload");
    }

    // move_to_liquid material Lava: a strider off the lava finds the nearest block topped by lava under air;
    // a shallow pool, the walk ends standing in the lava block
    void findsTheLava() {
        Arena arena(DimensionType::Nether);
        arena.player(1, Vector3f(-6.5f, 64.0f, -6.5f));
        for (int32_t x = 4; x <= 8; x++) {
            for (int32_t z = 0; z <= 4; z++) {
                arena.mLevel.setBlockState(x, 62, z, BlockState("minecraft:stone"));
                arena.mLevel.setBlockState(x, 63, z, BlockState("minecraft:lava"));
            }
        }
        MobActor &strider = arena.spawn("minecraft:strider", Vector3f(0.5f, 64.0f, 2.5f), "spawn_adult");
        BaseMoveToBlockGoal::Settings settings;
        settings.mSpeed = 0.16f;
        settings.mSearchRange = 16;
        settings.mSearchHeight = 10;
        settings.mGoalRadius = 0.9f;
        MoveToLiquidGoal lava(settings, MoveToLiquidGoal::Material::Lava);
        require(lava.canUse(arena.mOwner, strider), "a strider on stone found no lava to walk to");

        MoveToLiquidGoal any(settings, MoveToLiquidGoal::Material::Any);
        require(!any.canUse(arena.mOwner, strider), "the any material moved the strider");
        MoveToLiquidGoal water(settings, MoveToLiquidGoal::Material::Water);
        require(!water.canUse(arena.mOwner, strider), "the strider found water in a lava pool");

        strider.setPosition(Vector3f(6.5f, 63.0f, 2.5f));
        MoveToLiquidGoal inside(settings, MoveToLiquidGoal::Material::Lava);
        require(!inside.canUse(arena.mOwner, strider), "a strider standing in lava looks for lava");
    }

    void saddleAndStick() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 1.5f));
        MobActor &strider = arena.spawn("minecraft:strider", Vector3f(0.5f, 64.0f, 0.5f), "spawn_adult");
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:saddle", 1));
        require(InventoryHandler::interactWithEntity(arena.mOwner, player, strider.getRuntimeId(), Vector3f()),
                "the saddle was not used");
        arena.ticks(1);
        require(strider.getFlags().get(ActorFlag::Saddled), "the strider is not saddled");

        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:warped_fungus_on_a_stick", 1));
        require(RideSystem::mount(arena.mOwner, player, strider, true), "the player could not ride");
        arena.ticks(1);
        require(RideControlSystem::tryBoost(arena.mOwner, player), "the stick did not boost the strider");
        require(player.getInventory().getItemInHand().mDamage == 1, "a boost did not cost the stick 1 durability");

        const Tag data = strider.saveNbt();
        RideSystem::dismount(arena.mOwner, player, true);
        MobActor *loaded = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:strider", Vector3f(4.5f, 64.0f, 0.5f)));
        loaded->loadNbt(data);
        arena.ticks(1);
        require(loaded->getFlags().get(ActorFlag::Saddled), "the saddle was lost on reload");
    }

    void jockeys() {
        Arena arena(DimensionType::Nether);
        arena.player(1, Vector3f(-6.5f, 64.0f, -6.5f));
        MobActor &parent = arena.spawn("minecraft:strider", Vector3f(0.5f, 64.0f, 0.5f), "spawn_adult_parent_jockey");
        arena.ticks(2);
        require(parent.getPassengers().size() == 1, "the parent jockey carries no baby strider");
        MobActor &jockey = arena.spawn("minecraft:strider", Vector3f(4.5f, 64.0f, 0.5f), "spawn_adult_piglin_jockey");
        arena.ticks(2);
        require(jockey.getPassengers().size() == 1 && arena.countOf("minecraft:zombie_pigman") == 1,
                "the piglin jockey carries no zombified piglin");
        require(jockey.getFlags().get(ActorFlag::Saddled), "the piglin jockey's strider wears no saddle");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        risesToTheSurface();
        walksOnDeepLava();
        shiversOnLand();
        findsTheLava();
        saddleAndStick();
        jockeys();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "strider tests passed" << std::endl;
    return 0;
}
