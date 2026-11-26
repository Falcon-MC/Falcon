#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/EvocationFangActor.h"
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

    struct Arena {
        ServerNetworkHandler mOwner{"Projectile impacts", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            // wide: a homing bullet's random legs can carry it 100 blocks off, and a chunk it reads out of memory
            // would be generated with terrain
            for (int32_t x = -8; x <= 8; x++) {
                for (int32_t z = -8; z <= 8; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk
                    // wins the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
        }

        ServerActor &spawn(const char *identifier, const Vector3f &position) {
            ServerActor *actor = mOwner.spawnActor(*mLevel, identifier, position);
            require(actor != nullptr, std::string("could not spawn ") + identifier);
            return *actor;
        }

        bool alive(int64_t id) {
            return mOwner.getActors().count(id) == 1;
        }
    };

    // a thrown projectile only hits from its third tick: held still for two, then sent along +z
    ServerActor &launch(Arena &arena, const char *identifier, const Vector3f &from) {
        ServerActor &projectile = arena.spawn(identifier, from);
        projectile.setProjectile(true);
        for (int32_t tick = 0; tick < 2; ++tick) {
            projectile.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            projectile.setPosition(from);
            arena.mOwner.tickActors();
        }
        projectile.setMotion(Vector3f(0.0f, 0.0f, 0.5f));
        return projectile;
    }

    // flies into a cow standing in its way; true once the cow was struck
    float hitCow(Arena &arena, const char *identifier, float x, ServerActor *&cowOut, float height = 100.4f) {
        ServerActor &cow = arena.spawn("minecraft:cow", Vector3f(x, 100.0f, 6.5f));
        cowOut = &cow;
        const float before = cow.getHealth();
        ServerActor &projectile = launch(arena, identifier, Vector3f(x, height, 3.5f));
        const int64_t id = projectile.getUniqueId();
        // held where it stands: a falling or wandering cow can leave the projectile's path
        const Vector3f standing = cow.getPosition();
        for (int32_t tick = 0; tick < 20 && arena.alive(id); ++tick) {
            cow.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            cow.setPosition(standing);
            arena.mOwner.tickActors();
        }
        return before - cow.getHealth();
    }

    void smallFireball(Arena &arena) {
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:small_fireball", -20.5f, cow);
        require(dealt == 5.0f, "a small fireball did not deal 5, dealt " + std::to_string(dealt));
        require(cow->isOnFire(), "a small fireball did not set the cow alight");

        // into a wall: the cell it came through burns
        Level &level = *arena.mLevel;
        for (int32_t y = 99; y <= 102; ++y)
            level.setBlockState(-24, y, 8, BlockState("minecraft:stone"));
        level.setBlockState(-24, 99, 7, BlockState("minecraft:stone"));
        ServerActor &fireball = launch(arena, "minecraft:small_fireball", Vector3f(-23.5f, 100.4f, 4.5f));
        const int64_t id = fireball.getUniqueId();
        for (int32_t tick = 0; tick < 20 && arena.alive(id); ++tick)
            arena.mOwner.tickActors();
        require(level.getBlockState(-24, 100, 7).mName == "minecraft:fire",
                "a small fireball did not set fire before the wall, found "
                        + level.getBlockState(-24, 100, 7).mName);
    }

    void ghastFireball(Arena &arena) {
        Level &level = *arena.mLevel;
        for (int32_t y = 99; y <= 102; ++y) {
            for (int32_t x = 49; x <= 51; ++x)
                level.setBlockState(x, y, 9, BlockState("minecraft:dirt"));
        }
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:fireball", 50.5f, cow);
        require(dealt >= 6.0f, "a ghast fireball's impact dealt less than 6: " + std::to_string(dealt));

        ServerActor &fireball = launch(arena, "minecraft:fireball", Vector3f(50.5f, 100.4f, 4.5f));
        auto *held = dynamic_cast<ThrownProjectileActor *>(&fireball);
        require(held != nullptr, "the ghast fireball has no hit handling");
        for (int32_t tick = 0; tick < 20 && !held->isHeldAfterHit(); ++tick)
            arena.mOwner.tickActors();
        require(held->isHeldAfterHit(), "the ghast fireball did not stop to explode");
        arena.mOwner.tickActors();
        int32_t dirt = 0;
        for (int32_t y = 99; y <= 102; ++y) {
            for (int32_t x = 49; x <= 51; ++x)
                dirt += level.getBlockState(x, y, 9).mName == "minecraft:dirt" ? 1 : 0;
        }
        require(dirt < 12, "the ghast fireball's explosion broke nothing");
    }

    void llamaSpit(Arena &arena) {
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:llama_spit", 20.5f, cow, 101.4f);
        require(dealt == 1.0f, "llama spit did not deal 1, dealt " + std::to_string(dealt));
    }

    void shulkerBullet(Arena &arena) {
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:shulker_bullet", 30.5f, cow);
        require(dealt == 4.0f, "a shulker bullet did not deal 4, dealt " + std::to_string(dealt));
        require(cow->hasEffect(MobEffectId::Levitation), "a shulker bullet did not make the cow levitate");
    }

    void breezeWindCharge(Arena &arena) {
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:breeze_wind_charge_projectile", -30.5f, cow);
        require(dealt == 1.0f, "a breeze wind charge did not deal 1, dealt " + std::to_string(dealt));
    }

    void evocationFangs(Arena &arena) {
        ServerActor &summoner = arena.spawn("minecraft:evocation_illager", Vector3f(10.5f, 100.0f, -20.5f));
        ServerActor &cow = arena.spawn("minecraft:cow", Vector3f(10.5f, 100.0f, -10.5f));
        ServerActor &vex = arena.spawn("minecraft:vex", Vector3f(10.5f, 100.0f, -10.5f));
        vex.setOwnerUniqueId(summoner.getUniqueId());
        const float cowBefore = cow.getHealth();
        const float vexBefore = vex.getHealth();

        ServerActor &fang = arena.spawn("minecraft:evocation_fang", Vector3f(10.5f, 100.0f, -10.5f));
        require(dynamic_cast<EvocationFangActor *>(&fang) != nullptr, "the fang has no class of its own");
        require(!fang.isProjectile(), "the fang flies like a projectile");
        fang.setOwnerUniqueId(summoner.getUniqueId());
        fang.setLimitedLife(EvocationFangActor::LIFESPAN_TICKS);
        const int64_t id = fang.getUniqueId();

        arena.mOwner.tickActors();
        require(cow.getHealth() == cowBefore, "the fang bit before its warmup");
        arena.mOwner.tickActors();
        require(cowBefore - cow.getHealth() == EvocationFangActor::BITE_DAMAGE,
                "the fang did not bite the cow for 6, dealt " + std::to_string(cowBefore - cow.getHealth()));
        require(vex.getHealth() == vexBefore, "the fang bit its summoner's vex");
        for (int32_t tick = 0; tick < 4; ++tick)
            arena.mOwner.tickActors();
        require(cowBefore - cow.getHealth() == EvocationFangActor::BITE_DAMAGE, "the fang bit more than once");
        for (int32_t tick = 0; tick < 30 && arena.alive(id); ++tick)
            arena.mOwner.tickActors();
        require(!arena.alive(id), "the fang outlived its life");
    }

    float distance(const Vector3f &a, const Vector3f &b) {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    DefinitionProjectileActor &definitionProjectile(Arena &arena, const char *identifier, const Vector3f &at) {
        ServerActor &actor = arena.spawn(identifier, at);
        actor.setProjectile(true);
        actor.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        auto *projectile = dynamic_cast<DefinitionProjectileActor *>(&actor);
        require(projectile != nullptr, std::string(identifier) + " has no definition handling");
        return *projectile;
    }

    // a shulker bullet weaves its way to a cow six blocks off and lands on it
    void shulkerHoming(Arena &arena) {
        Level &level = *arena.mLevel;
        for (int32_t x = 38; x <= 42; ++x) {
            for (int32_t z = -36; z <= -24; ++z)
                level.setBlockState(x, 99, z, BlockState("minecraft:stone"));
        }
        // the shooter stays out of the way: a leg drawn at random may fly back past the start
        ServerActor &shulker = arena.spawn("minecraft:cow", Vector3f(20.5f, 100.0f, -45.5f));
        ServerActor &cow = arena.spawn("minecraft:cow", Vector3f(40.5f, 100.0f, -25.5f));
        const float before = cow.getHealth();
        DefinitionProjectileActor &bullet =
                definitionProjectile(arena, "minecraft:shulker_bullet", Vector3f(40.5f, 101.0f, -33.5f));
        bullet.setOwnerUniqueId(shulker.getUniqueId());
        bullet.launchAt(arena.mOwner, shulker, cow);
        const int64_t id = bullet.getUniqueId();
        // most bullets land in 40 ticks; lined up with the cow on two axes, the next leg is random and can fly the
        // other way: 1 in 250 takes over 300 ticks, 2400 leave a miss under 1e-8
        for (int32_t tick = 0; tick < 2400 && arena.alive(id); ++tick)
            arena.mOwner.tickActors();
        require(!arena.alive(id), "the shulker bullet never reached the cow");
        require(cow.hasEffect(MobEffectId::Levitation),
                "the shulker bullet did not home onto the cow, dealt " + std::to_string(before - cow.getHealth()));
    }

    // its target gone, the bullet falls instead of flying on
    void shulkerTargetLost(Arena &arena) {
        ServerActor &shulker = arena.spawn("minecraft:cow", Vector3f(-40.5f, 100.0f, -35.5f));
        ServerActor &cow = arena.spawn("minecraft:cow", Vector3f(-40.5f, 100.0f, -20.5f));
        DefinitionProjectileActor &bullet =
                definitionProjectile(arena, "minecraft:shulker_bullet", Vector3f(-40.5f, 110.0f, -33.5f));
        bullet.launchAt(arena.mOwner, shulker, cow);
        arena.mOwner.removeActor(cow.getUniqueId());
        const float startY = bullet.getPosition().y;
        for (int32_t tick = 0; tick < 5; ++tick)
            arena.mOwner.tickActors();
        require(bullet.getMotion().y < -0.15f, "a bullet without a target does not fall, motion y "
                                                       + std::to_string(bullet.getMotion().y));
        require(bullet.getPosition().y < startY, "a bullet without a target did not drop");
    }

    // a homing bullet no one aimed falls 0.04 a tick, without the definition's inertia and gravity
    void shulkerUnaimed(Arena &arena) {
        DefinitionProjectileActor &bullet = definitionProjectile(arena, "minecraft:shulker_bullet",
                                                                 Vector3f(-20.5f, 110.0f, -10.5f));
        for (int32_t tick = 0; tick < 2; ++tick) {
            bullet.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            arena.mOwner.tickActors();
        }
        bullet.setMotion(Vector3f(0.0f, 0.0f, 0.5f));
        arena.mOwner.tickActors();
        const Vector3f motion = bullet.getMotion();
        require(distance(motion, Vector3f(0.0f, -0.04f, 0.5f)) < 1.0e-6f,
                "an unaimed shulker bullet did not fall like a homing one, motion " + std::to_string(motion.x) + " "
                        + std::to_string(motion.y) + " " + std::to_string(motion.z));
        arena.mOwner.removeActor(bullet.getUniqueId());
    }

    // a breeze sends a snowball back from where it met it, its new motion slowed and pulled down that same tick
    void breezeReflects(Arena &arena) {
        ServerActor &breeze = arena.spawn("minecraft:breeze", Vector3f(-30.5f, 100.0f, 30.5f));
        const Vector3f standing = breeze.getPosition();
        ServerActor &snowball = launch(arena, "minecraft:snowball", Vector3f(-30.5f, 100.9f, 26.5f));
        const float gravity = snowball.getProjectileComponent().mGravity;
        const float inertia = snowball.getProjectileComponent().mInertia;
        const int64_t id = snowball.getUniqueId();
        Vector3f before = snowball.getMotion();
        for (int32_t tick = 0; tick < 20 && arena.alive(id) && snowball.getMotion().z > 0.0f; ++tick) {
            before = snowball.getMotion();
            breeze.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            breeze.setPosition(standing);
            arena.mOwner.tickActors();
        }
        require(arena.alive(id) && snowball.getMotion().z < 0.0f, "the breeze did not send the snowball back");
        require(snowball.getPosition().z < standing.z, "the reflected snowball went on through the breeze");
        const Vector3f after = snowball.getMotion();
        const float kept = std::sqrt(after.x * after.x + after.z * after.z);
        const float expected = std::sqrt(before.x * before.x + before.z * before.z) * 0.5f * inertia;
        require(std::fabs(kept - expected) < 1.0e-4f,
                "the reflected motion was not slowed by inertia: " + std::to_string(kept) + " for "
                        + std::to_string(expected));
        require(std::fabs(after.y - (before.y * 0.5f * inertia - gravity)) < 1.0e-4f,
                "the reflected motion did not fall that tick: " + std::to_string(after.y));
        arena.mOwner.removeActor(id);
    }

    // a punch breaks a shulker bullet
    void shulkerPunched(Arena &arena) {
        ServerActor &attacker = arena.spawn("minecraft:cow", Vector3f(-10.5f, 100.0f, 40.5f));
        DefinitionProjectileActor &bullet =
                definitionProjectile(arena, "minecraft:shulker_bullet", Vector3f(-10.5f, 101.0f, 42.5f));
        const int64_t id = bullet.getUniqueId();
        require(bullet.onMeleeHit(arena.mOwner, attacker), "a punch did nothing to the shulker bullet");
        require(!arena.alive(id), "a punched shulker bullet survived");
    }

    // a punched ghast fireball flies where the attacker looks, now the attacker's
    void fireballReflected(Arena &arena) {
        ServerActor &attacker = arena.spawn("minecraft:cow", Vector3f(10.5f, 100.0f, 40.5f));
        // yaw 90: looking toward -x
        attacker.setRotation(Vector3f(0.0f, 90.0f, 0.0f));
        DefinitionProjectileActor &fireball =
                definitionProjectile(arena, "minecraft:fireball", Vector3f(10.5f, 101.0f, 42.5f));
        fireball.setMotion(Vector3f(0.0f, 0.0f, -0.5f));
        require(fireball.onMeleeHit(arena.mOwner, attacker), "a punch did not reflect the fireball");
        const Vector3f motion = fireball.getMotion();
        require(distance(motion, Vector3f(-1.0f, 0.0f, 0.0f)) < 1.0e-4f,
                "the fireball did not fly where the attacker looks");
        require(fireball.getOwnerUniqueId() == attacker.getUniqueId(), "the reflected fireball kept its owner");
        arena.mOwner.removeActor(fireball.getUniqueId());
    }

    // a fresh wind charge shrugs punches off until reflect_immunity's half second has passed
    void windChargeImmunity(Arena &arena) {
        ServerActor &attacker = arena.spawn("minecraft:cow", Vector3f(20.5f, 100.0f, 40.5f));
        DefinitionProjectileActor &charge =
                definitionProjectile(arena, "minecraft:wind_charge_projectile", Vector3f(20.5f, 110.0f, 42.5f));
        require(!charge.onMeleeHit(arena.mOwner, attacker), "a fresh wind charge was reflected");
        for (int32_t tick = 0; tick < 10; ++tick) {
            charge.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            arena.mOwner.tickActors();
        }
        require(charge.onMeleeHit(arena.mOwner, attacker), "a wind charge past its immunity was not reflected");
        arena.mOwner.removeActor(charge.getUniqueId());
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-impacts-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        smallFireball(arena);
        ghastFireball(arena);
        llamaSpit(arena);
        shulkerBullet(arena);
        breezeWindCharge(arena);
        evocationFangs(arena);
        shulkerHoming(arena);
        shulkerTargetLost(arena);
        shulkerUnaimed(arena);
        shulkerPunched(arena);
        fireballReflected(arena);
        windChargeImmunity(arena);
        breezeReflects(arena);
    } catch (const std::exception &error) {
        std::cout << "Projectile impact regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
