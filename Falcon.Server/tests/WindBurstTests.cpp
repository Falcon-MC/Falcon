#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Projectile/ProjectileReflection.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/WindBurst.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool near(float value, float expected, float tolerance = 0.001f) {
        return std::fabs(value - expected) <= tolerance;
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Wind burst", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 1; x++) {
                for (int32_t z = -2; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        ServerActor &spawn(const char *identifier, const Vector3f &position) {
            ServerActor *actor = mOwner.spawnActor(mLevel, identifier, position);
            require(actor != nullptr, std::string(identifier) + " did not spawn");
            return *actor;
        }
    };

    WindBurst::Settings playerCharge() {
        WindBurst::Settings settings;
        settings.mPower = 1.2f;
        settings.mKnockbackScaling = 1.22f;
        settings.mTogglesBlocks = true;
        settings.mNegatesFallDamage = true;
        settings.mAllowUnderwater = true;
        return settings;
    }

    BlockState trapdoor(bool open) {
        BlockState state("minecraft:trapdoor");
        state.mStates.putInt("direction", 0);
        state.mStates.putByte("open_bit", open ? 1 : 0);
        state.mStates.putByte("upside_down_bit", 0);
        return state;
    }

    BlockState candle(bool lit) {
        BlockState state("minecraft:candle");
        state.mStates.putInt("candles", 0);
        state.mStates.putByte("lit", lit ? 1 : 0);
        return state;
    }

    void impacts() {
        WindBurst::Settings settings = playerCharge();
        require(near(WindBurst::impact(0.0f, 1.0f, settings), 1.22f), "a burst on the spot does not push by knockback_scaling");
        require(near(WindBurst::impact(1.2f, 1.0f, settings), 0.61f), "half the reach does not push half as hard");
        require(near(WindBurst::impact(1.2f, 0.5f, settings), 0.305f), "a half hidden actor is not pushed half as hard");
        require(WindBurst::impact(2.5f, 1.0f, settings) == 0.0f, "an actor beyond twice the power was pushed");
    }

    void pushes() {
        Arena arena;
        ServerActor &pig = arena.spawn("minecraft:pig", Vector3f(2.0f, 100.0f, 0.5f));
        ServerActor &far = arena.spawn("minecraft:pig", Vector3f(9.5f, 100.0f, 0.5f));
        WindBurst::Settings breeze;
        breeze.mPower = 3.0f;
        breeze.mKnockbackScaling = 0.6f;
        WindBurst::burst(arena.mOwner, arena.mLevel, Vector3f(0.5f, 100.5f, 0.5f), breeze, nullptr);
        require(pig.getMotion().x > 0.0f, "the pig was not pushed away from the burst");
        require(far.getMotion().x == 0.0f, "a pig out of reach was pushed");
    }

    void toggles() {
        Arena arena;
        const Vector3i hatch(1, 100, 0);
        const Vector3i wick(0, 100, 1);
        arena.mLevel.setBlockState(hatch.x, hatch.y, hatch.z, trapdoor(false));
        arena.mLevel.setBlockState(wick.x, wick.y, wick.z, candle(true));
        const Vector3f center(0.5f, 100.5f, 0.5f);

        // a mob's charge leaves blocks alone without mobGriefing
        arena.mLevel.getGameRules().setFromString("mobgriefing", "false");
        WindBurst::Settings mob = playerCharge();
        mob.mNeedsMobGriefing = true;
        WindBurst::burst(arena.mOwner, arena.mLevel, center, mob, nullptr);
        require(arena.mLevel.getBlockState(hatch.x, hatch.y, hatch.z).mStates.getByte("open_bit", 0) == 0,
                "a mob's burst opened a trapdoor without mobGriefing");

        WindBurst::burst(arena.mOwner, arena.mLevel, center, playerCharge(), nullptr);
        require(arena.mLevel.getBlockState(hatch.x, hatch.y, hatch.z).mStates.getByte("open_bit", 0) == 1,
                "the burst did not open the trapdoor beside it");
        require(arena.mLevel.getBlockState(wick.x, wick.y, wick.z).mStates.getByte("lit", 1) == 0,
                "the burst did not blow the candle out");

        std::mt19937 random(3);
        const std::vector<Vector3i> reached =
                WindBurst::reachedBlocks(arena.mLevel, Vector3f(0.5f, 100.5f, 0.5f), playerCharge(), random);
        for (const Vector3i &block: reached)
            require(std::abs(block.x) <= 3 && std::abs(block.z) <= 3, "a ray of a small burst went too far");
    }

    void reflections() {
        const Vector3f back = ProjectileReflection::turned(Vector3f(1.0f, 0.0f, 0.0f), 180.0f, 0.0f, 0.5f);
        require(near(back.x, -0.5f) && near(back.z, 0.0f) && near(back.y, 0.0f), "a half turn did not send it back at half speed");
        const Vector3f raised = ProjectileReflection::turned(Vector3f(0.0f, 0.0f, 1.0f), 0.0f, 90.0f, 1.0f);
        require(near(raised.y, 1.0f), "the elevation did not raise the projectile");

        Arena arena;
        ServerActor &breeze = arena.spawn("minecraft:breeze", Vector3f(0.5f, 100.0f, 0.5f));
        ServerActor &snowball = arena.spawn("minecraft:snowball", Vector3f(-1.0f, 101.0f, 0.5f));
        snowball.setMotion(Vector3f(1.0f, 0.0f, 0.0f));
        require(ProjectileReflection::reflect(arena.mOwner, arena.mLevel, snowball, breeze), "a snowball was not reflected");
        const Vector3f motion = snowball.getMotion();
        require(motion.x < 0.0f, "the reflected snowball still flies at the breeze");
        const float speed = std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z);
        require(near(speed, 0.5f), "reflection_scale did not slow the snowball");

        ServerActor &charge = arena.spawn("minecraft:wind_charge_projectile", Vector3f(-1.0f, 101.0f, 0.5f));
        charge.setMotion(Vector3f(1.0f, 0.0f, 0.0f));
        require(!ProjectileReflection::reflect(arena.mOwner, arena.mLevel, charge, breeze),
                "a projectile left out of reflected_projectiles was reflected");
        ServerActor &pig = arena.spawn("minecraft:pig", Vector3f(4.5f, 100.0f, 0.5f));
        require(!ProjectileReflection::reflect(arena.mOwner, arena.mLevel, snowball, pig), "a pig reflected a snowball");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-wind-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        impacts();
        pushes();
        toggles();
        reflections();
        std::cout << "wind burst checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
