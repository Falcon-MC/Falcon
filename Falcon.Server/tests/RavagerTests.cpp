#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t TICKS_PER_SECOND = 20;
    const char *const RAVAGER = "minecraft:ravager";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Ravagers", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
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

        // a shield in the off hand, raised toward +z
        ServerPlayer &blocker(uint64_t runtimeId, const Vector3f &position) {
            ServerPlayer &player = this->player(runtimeId, position);
            player.getInventory().setOffhand(mOwner.createItemStack("minecraft:shield", 1));
            player.setRotation(Vector3f(0.0f, 0.0f, 0.0f));
            player.getFlags().set(ActorFlag::Blocking, true);
            return player;
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void tick() {
            mOwner.getWorlds().getDefault().getOverworld().tickTime();
            PathFinder::tryReserveSearch(-1);
            mOwner.tickActors();
        }
    };

    // knockback_strength 3, scaled to motion as the roar's strengths are
    void blockerPushedOff() {
        Arena arena;
        ServerPlayer &player = arena.blocker(1, Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &ravager = arena.spawn(RAVAGER, Vector3f(0.5f, 64.0f, 3.5f), "minecraft:entity_spawned");
        ravager.onAttackBlocked(arena.mOwner, player);
        const Vector3f motion = player.getMotion();
        require(std::fabs(motion.x) < 0.001f && std::fabs(motion.z + 1.2f) < 0.001f,
                "the blocker was not pushed away from the ravager, z " + std::to_string(motion.z));

        // without minecraft:ravager_blocked a blocked hit pushes nothing and fires nothing
        player.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(0.5f, 64.0f, 3.5f), "minecraft:entity_spawned");
        zombie.onAttackBlocked(arena.mOwner, player);
        require(player.getMotion().z == 0.0f, "a zombie's blocked hit pushed the blocker");
    }

    // reaction_choices: become_stunned or nothing, one weight each
    void reactionsAreAnEvenDraw() {
        Arena arena;
        ServerPlayer &player = arena.blocker(1, Vector3f(0.5f, 64.0f, 0.5f));
        int32_t stuns = 0;
        const int32_t tries = 400;
        for (int32_t attempt = 0; attempt < tries; attempt++) {
            MobActor &ravager = arena.spawn(RAVAGER, Vector3f(0.5f, 64.0f, 3.5f), "minecraft:entity_spawned");
            ravager.onAttackBlocked(arena.mOwner, player);
            stuns += ravager.hasComponentGroup("stunned") ? 1 : 0;
            arena.mOwner.removeActor(ravager.getUniqueId());
        }
        require(stuns > tries / 4 && stuns < tries * 3 / 4,
                "a blocked bite stunned " + std::to_string(stuns) + " of " + std::to_string(tries) + " ravagers");
    }

    // the whole loop through the ravager's own goals: blocked bites until one stuns, two seconds stunned,
    // the roar, then back to hunting
    void stunThenRoar() {
        Arena arena;
        ServerPlayer &player = arena.blocker(1, Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &ravager = arena.spawn(RAVAGER, Vector3f(0.5f, 64.0f, 3.0f), "minecraft:entity_spawned");
        const float health = player.getHealth();

        int32_t ticks = 0;
        while (!ravager.hasComponentGroup("stunned") && ticks++ < 60 * TICKS_PER_SECOND) {
            // the shield stays up and the player stands still: the client moves players, not the server
            player.setPosition(Vector3f(0.5f, 64.0f, 0.5f));
            player.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            if (ravager.getTarget(arena.mOwner) == nullptr)
                ravager.setTarget(arena.mOwner, player.getRuntimeId());
            arena.tick();
        }
        require(ravager.hasComponentGroup("stunned"), "no blocked bite ever stunned the ravager");
        require(player.getHealth() == health, "a blocked bite hurt the player");
        require(!ravager.hasComponentGroup("minecraft:hostile"), "the stunned ravager kept hunting");
        arena.tick();
        require(ravager.getFlags().get(ActorFlag::Stunned), "the stunned flag was not shown");

        // the stunned group's timer: two seconds, then the roar
        player.getFlags().set(ActorFlag::Blocking, false);
        for (int32_t tick = 0; tick < 2 * TICKS_PER_SECOND + 2 && !ravager.hasComponentGroup("roaring"); tick++)
            arena.tick();
        require(ravager.hasComponentGroup("roaring") && !ravager.hasComponentGroup("stunned"),
                "the ravager did not roar two seconds after its stun");
        arena.tick();
        require(!ravager.getFlags().get(ActorFlag::Stunned), "the stunned flag outlived the stun");

        for (int32_t tick = 0; tick < 2 * TICKS_PER_SECOND && !ravager.hasComponentGroup("minecraft:hostile"); tick++)
            arena.tick();
        require(ravager.hasComponentGroup("minecraft:hostile") && !ravager.hasComponentGroup("roaring"),
                "the ravager did not go back to hunting after its roar");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        blockerPushedOff();
        reactionsAreAnEvenDraw();
        stunThenRoar();
    } catch (const std::exception &error) {
        std::clog << "Ravager regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ravager checks passed\n";
    return 0;
}
