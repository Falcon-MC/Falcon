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
            for (int32_t x = -3; x <= 3; x++) {
                for (int32_t z = -3; z <= 3; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
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
    float hitCow(Arena &arena, const char *identifier, float x, ServerActor *&cowOut) {
        ServerActor &cow = arena.spawn("minecraft:cow", Vector3f(x, 100.0f, 6.5f));
        cowOut = &cow;
        const float before = cow.getHealth();
        ServerActor &projectile = launch(arena, identifier, Vector3f(x, 100.4f, 3.5f));
        const int64_t id = projectile.getUniqueId();
        for (int32_t tick = 0; tick < 20 && arena.alive(id); ++tick)
            arena.mOwner.tickActors();
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
            for (int32_t x = -1; x <= 1; ++x)
                level.setBlockState(x, y, 9, BlockState("minecraft:dirt"));
        }
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:fireball", 0.5f, cow);
        require(dealt >= 6.0f, "a ghast fireball's impact dealt less than 6: " + std::to_string(dealt));

        ServerActor &fireball = launch(arena, "minecraft:fireball", Vector3f(0.5f, 100.4f, 4.5f));
        auto *held = dynamic_cast<ThrownProjectileActor *>(&fireball);
        require(held != nullptr, "the ghast fireball has no hit handling");
        for (int32_t tick = 0; tick < 20 && !held->isHeldAfterHit(); ++tick)
            arena.mOwner.tickActors();
        require(held->isHeldAfterHit(), "the ghast fireball did not stop to explode");
        arena.mOwner.tickActors();
        int32_t dirt = 0;
        for (int32_t y = 99; y <= 102; ++y) {
            for (int32_t x = -1; x <= 1; ++x)
                dirt += level.getBlockState(x, y, 9).mName == "minecraft:dirt" ? 1 : 0;
        }
        require(dirt < 12, "the ghast fireball's explosion broke nothing");
    }

    void llamaSpit(Arena &arena) {
        ServerActor *cow = nullptr;
        const float dealt = hitCow(arena, "minecraft:llama_spit", 20.5f, cow);
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
    } catch (const std::exception &error) {
        std::cout << "Projectile impact regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
