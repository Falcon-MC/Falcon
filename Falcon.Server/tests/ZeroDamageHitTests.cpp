#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // resistance 255 takes the whole hit
    const int32_t FULL_RESISTANCE_AMPLIFIER = 254;
    const int32_t HURT_WINDOW_TICKS = 10;

    struct Arena {
        ServerNetworkHandler mOwner{"Zero damage", "", 1};
        Level *mLevel = nullptr;
        int32_t mPlayerHurts = 0;
        float mLastHurtDamage = -1.0f;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "hard.properties";
            std::ofstream(path) << "difficulty=hard\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
            mOwner.getEventBus().after().mEntityHurt.subscribe([this](EntityHurtAfterEvent &event) {
                ++mPlayerHurts;
                mLastHurtDamage = event.mDamage;
            });
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

        MobActor &mob(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        template <typename T>
        T &projectile(const char *identifier, const Vector3f &position) {
            T *projectile = dynamic_cast<T *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(projectile != nullptr, std::string(identifier) + " did not spawn");
            projectile->setMotion(Vector3f(1.0f, 0.0f, 0.0f));
            return *projectile;
        }
    };

    bool pushed(const Actor &actor) {
        const Vector3f motion = actor.getMotion();
        return std::fabs(motion.x) > 0.0f || std::fabs(motion.z) > 0.0f;
    }

    void requireHurtForNothing(Arena &arena, ServerPlayer &player, float health, int32_t hurtsBefore,
                               const std::string &what) {
        require(player.getHealth() == health, what + ": the player lost health");
        require(arena.mPlayerHurts == hurtsBefore + 1 && arena.mLastHurtDamage == 0.0f,
                what + ": no hurt for 0 was reported");
        require(player.getNoDamageTicks() == HURT_WINDOW_TICKS, what + ": the hurt window did not start");
        require(pushed(player), what + ": the player was not knocked back");
    }

    void snowballOnPlayer(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &player = arena.player(9001, Vector3f(1.5f, 100.0f, 0.5f));
        const float health = player.getHealth();
        auto &snowball = arena.projectile<SnowballActor>("minecraft:snowball", Vector3f(0.5f, 101.0f, 0.5f));
        snowball.onHit(arena.mOwner, Vector3f(1.2f, 101.0f, 0.5f), &player);
        requireHurtForNothing(arena, player, health, 0, "snowball");
    }

    void eggOnPlayer(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &player = arena.player(9002, Vector3f(1.5f, 100.0f, 0.5f));
        const float health = player.getHealth();
        auto &egg = arena.projectile<EggActor>("minecraft:egg", Vector3f(0.5f, 101.0f, 0.5f));
        egg.onHit(arena.mOwner, Vector3f(1.2f, 101.0f, 0.5f), &player);
        requireHurtForNothing(arena, player, health, 0, "egg");
    }

    void snowballOnMob(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &zombie = arena.mob("minecraft:zombie", Vector3f(1.5f, 100.0f, 0.5f));
        const float health = zombie.getHealth();
        auto &snowball = arena.projectile<SnowballActor>("minecraft:snowball", Vector3f(0.5f, 101.0f, 0.5f));
        snowball.onHitActor(arena.mOwner, Vector3f(1.2f, 101.0f, 0.5f), zombie);
        require(zombie.getHealth() == health, "snowball: the zombie lost health");
        require(zombie.getNoDamageTicks() == HURT_WINDOW_TICKS, "snowball: the zombie did not flinch");
        require(pushed(zombie), "snowball: the zombie was not knocked back");
    }

    // the mob turns on the thrower and is pushed along the throw, not away from the thrower
    void thrownByPlayer(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &thrower = arena.player(9006, Vector3f(-5.5f, 100.0f, 0.5f));
        thrower.setUniqueId(424242);
        MobActor &zombie = arena.mob("minecraft:zombie", Vector3f(0.5f, 100.0f, 0.5f));
        auto &snowball = arena.projectile<SnowballActor>("minecraft:snowball", Vector3f(0.5f, 101.0f, -0.5f));
        snowball.setOwnerUniqueId(thrower.getUniqueId());
        snowball.setMotion(Vector3f(0.0f, 0.0f, 1.0f));
        snowball.onHitActor(arena.mOwner, Vector3f(0.5f, 101.0f, 0.2f), zombie);
        require(zombie.getLastHurtBy() == thrower.getRuntimeId(), "thrown: the zombie did not note its thrower");
        require(thrower.getLastAttackedRuntimeId() == zombie.getRuntimeId(), "thrown: the throw was not the thrower's");
        const Vector3f motion = zombie.getMotion();
        require(motion.z > 0.0f && std::fabs(motion.x) < 0.0001f,
                "thrown: the zombie was pushed away from the thrower");
    }

    void resistantPlayerHitByPlayer(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &attacker = arena.player(9003, Vector3f(0.5f, 100.0f, 0.5f));
        ServerPlayer &victim = arena.player(9004, Vector3f(1.5f, 100.0f, 0.5f));
        arena.mOwner.addEffect(victim, MobEffectId::Resistance, FULL_RESISTANCE_AMPLIFIER, 1000);
        const float health = victim.getHealth();
        require(attacker.attackActor(arena.mOwner, victim.getRuntimeId(), nullptr, false),
                "resistance: the punch did not land");
        require(victim.getHealth() == health, "resistance: the punched player lost health");
        require(arena.mPlayerHurts == 1, "resistance: the punch was not reported as a hurt");
        require(victim.getNoDamageTicks() == HURT_WINDOW_TICKS, "resistance: the hurt window did not start");
        require(pushed(victim), "resistance: the punched player was not knocked back");
    }

    void resistantPlayerHitByMob(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &zombie = arena.mob("minecraft:zombie", Vector3f(0.5f, 100.0f, 0.5f));
        ServerPlayer &victim = arena.player(9005, Vector3f(1.5f, 100.0f, 0.5f));
        arena.mOwner.addEffect(victim, MobEffectId::Resistance, FULL_RESISTANCE_AMPLIFIER, 1000);
        const float health = victim.getHealth();
        require(MobAttack::hit(arena.mOwner, zombie, victim, 3.0f), "resistance: the zombie's hit did not land");
        require(victim.getHealth() == health, "resistance: the zombie's hit took health");
        require(arena.mPlayerHurts == 1 && arena.mLastHurtDamage == 0.0f,
                "resistance: the zombie's hit was not reported as a hurt for 0");
        require(victim.getNoDamageTicks() == HURT_WINDOW_TICKS, "resistance: the hurt window did not start");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-zero-damage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        snowballOnPlayer(directory);
        eggOnPlayer(directory);
        snowballOnMob(directory);
        thrownByPlayer(directory);
        resistantPlayerHitByPlayer(directory);
        resistantPlayerHitByMob(directory);
        std::cout << "zero damage hit checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
