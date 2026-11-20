#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/KnockbackRules.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/MobEffect.h"
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
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool closeTo(float value, float expected) {
        return std::fabs(value - expected) < 0.0001f;
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Mob components", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "components.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, "mob did not spawn");
            return *mob;
        }
    };

    void effectImmunity(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &parched = arena.spawn("minecraft:parched", Vector3f(0.5f, 100.0f, 0.5f));
        require(!parched.canBeAffected(MobEffectId::Weakness), "the parched took weakness");
        require(parched.canBeAffected(MobEffectId::Poison), "the parched refused poison");

        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(2.5f, 100.0f, 0.5f));
        require(zombie.canBeAffected(MobEffectId::Weakness), "a zombie refused weakness");
    }

    void blockClimber(const std::filesystem::path &directory) {
        Arena arena(directory);
        require(arena.spawn("minecraft:endermite", Vector3f(0.5f, 100.0f, 0.5f)).getPhysics().mBlockClimber,
                "the endermite is no block climber");
        require(arena.spawn("minecraft:silverfish", Vector3f(2.5f, 100.0f, 0.5f)).getPhysics().mBlockClimber,
                "the silverfish is no block climber");
        require(!arena.spawn("minecraft:zombie", Vector3f(4.5f, 100.0f, 0.5f)).getPhysics().mBlockClimber,
                "a zombie became a block climber");
    }

    void drownedKnockback(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &drowned = arena.spawn("minecraft:drowned", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &dry = arena.spawn("minecraft:zombie", Vector3f(2.5f, 100.0f, 0.5f));
        KnockbackPreset preset;
        require(!KnockbackRules::select(arena.mOwner, drowned, dry, preset), "a target on land took the water preset");

        for (int32_t y = 100; y <= 103; y++)
            arena.mLevel->setBlockState(5, y, 0, BlockState("minecraft:water"));
        MobActor &swimmer = arena.spawn("minecraft:zombie", Vector3f(5.5f, 100.0f, 0.5f));
        require(KnockbackRules::isImmersedInWater(arena.mOwner, swimmer), "the zombie in water is not immersed");
        require(KnockbackRules::select(arena.mOwner, drowned, swimmer, preset), "the water preset was not picked");
        require(closeTo(preset.mHorizontalPower, 0.6f) && closeTo(preset.mVerticalPower, -0.6f)
                        && closeTo(preset.mVerticalVelocityCap, -0.4f),
                "the drowned preset was misread");

        swimmer.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        swimmer.knockBack(1.0f, 0.0f, preset.mHorizontalPower, preset.mVerticalPower, preset.mVerticalVelocityCap,
                          preset.mSlowdownScale);
        const Vector3f motion = swimmer.getMotion();
        require(closeTo(motion.x, 0.6f) && closeTo(motion.z, 0.0f), "the push away was not the horizontal power");
        require(closeTo(motion.y, -0.72f), "the pull down was not the vertical power times 1.2");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-components-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        effectImmunity(directory);
        blockClimber(directory);
        drownedKnockback(directory);
        std::cout << "mob component checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
