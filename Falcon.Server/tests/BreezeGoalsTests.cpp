#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/FireAtTargetGoal.h"
#include "Core/Math/AxisAlignedBB.h"
#include "World/Actor/AI/Goal/JumpAroundTargetGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

// a test server does not listen, so its tick returns at once: the clock is moved by hand
class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    const char *const WIND_CHARGE = "minecraft:breeze_wind_charge_projectile";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool near(float value, float expected, float tolerance = 0.001f) {
        return std::fabs(value - expected) <= tolerance;
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Breeze goals", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 1; x++) {
                for (int32_t z = -2; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        void floor(int32_t y, int32_t radius) {
            for (int32_t x = -radius; x <= radius; x++) {
                for (int32_t z = -radius; z <= radius; z++)
                    mLevel.setBlockState(x, y, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        int32_t count(const std::string &identifier) {
            int32_t found = 0;
            for (const auto &entry: mOwner.getActors()) {
                if (entry.second->getIdentifier() == identifier)
                    found++;
            }
            return found;
        }

        ServerActor *last(const std::string &identifier) {
            for (const auto &entry: mOwner.getActors()) {
                if (entry.second->getIdentifier() == identifier)
                    return entry.second.get();
            }
            return nullptr;
        }
    };

    FireAtTargetGoal::Settings breezeShot() {
        FireAtTargetGoal::Settings settings;
        settings.mProjectile = WIND_CHARGE;
        settings.mMaxRangeSquared = 16.0f * 16.0f;
        settings.mCooldown = 10;
        settings.mPreShootDelay = 15;
        settings.mPostShootDelay = 4;
        settings.mOwnerAnchor = FireAtTargetGoal::Anchor::Middle;
        settings.mOwnerOffset = Vector3f(0.0f, 0.3f, 0.0f);
        settings.mTargetAnchor = FireAtTargetGoal::Anchor::Feet;
        settings.mTargetOffset = Vector3f(0.0f, 0.5f, 0.0f);
        return settings;
    }

    void anchors() {
        Arena arena;
        MobActor &breeze = arena.spawn("minecraft:breeze", Vector3f(0.5f, 100.0f, 0.5f));
        const ActorSize size = breeze.getSize();
        const Vector3f middle = FireAtTargetGoal::anchorPoint(breeze, FireAtTargetGoal::Anchor::Middle,
                                                              Vector3f(0.0f, 0.3f, 0.0f));
        require(near(middle.y, 100.0f + size.mHeight * 0.8f), "the owner anchor is not the middle raised by 0.3 heights");
        require(near(middle.x, 0.5f) && near(middle.z, 0.5f), "a vertical offset moved the anchor sideways");
        const Vector3f feet = FireAtTargetGoal::anchorPoint(breeze, FireAtTargetGoal::Anchor::Feet, Vector3f());
        require(near(feet.y, 100.0f), "the feet anchor is not at the feet");
    }

    void fires() {
        Arena arena;
        MobActor &breeze = arena.spawn("minecraft:breeze", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(8.5f, 100.0f, 0.5f));
        require(breeze.setTarget(arena.mOwner, pig.getRuntimeId()), "the breeze could not target the pig");

        FireAtTargetGoal goal(breezeShot());
        require(goal.canUse(arena.mOwner, breeze), "a target in attack_range was not shot at");
        goal.start(arena.mOwner, breeze);
        require(breeze.getFlags().get(ActorFlag::FacingTargetToRangeAttack), "the facing flag was not set");

        const int32_t before = arena.count(WIND_CHARGE);
        for (int32_t tick = 0; tick < 15; tick++) {
            goal.tick(arena.mOwner, breeze);
            ServerTickProbe::advance(arena.mOwner, 1);
        }
        require(arena.count(WIND_CHARGE) == before, "the shot came before pre_shoot_delay");
        goal.tick(arena.mOwner, breeze);
        require(arena.count(WIND_CHARGE) == before + 1, "no wind charge at pre_shoot_delay");
        const ServerActor *charge = arena.last(WIND_CHARGE);
        require(charge != nullptr && charge->getMotion().x > 0.5f, "the wind charge does not fly at the target");
        require(charge->getMotion().y < 0.0f, "the wind charge does not aim down at the target's feet");
        goal.tick(arena.mOwner, breeze);
        require(arena.count(WIND_CHARGE) == before + 1, "the goal shot twice");

        // the last tick of post_shoot_delay still counts
        ServerTickProbe::advance(arena.mOwner, 4);
        require(goal.canContinueToUse(arena.mOwner, breeze), "the goal ended before post_shoot_delay ran out");
        ServerTickProbe::advance(arena.mOwner, 1);
        require(!goal.canContinueToUse(arena.mOwner, breeze), "the goal outlived post_shoot_delay");
        goal.stop(arena.mOwner, breeze);
        require(!breeze.getFlags().get(ActorFlag::FacingTargetToRangeAttack), "the facing flag stayed set");
        require(!goal.canUse(arena.mOwner, breeze), "shot again inside attack_cooldown");
        ServerTickProbe::advance(arena.mOwner, 10);
        require(goal.canUse(arena.mOwner, breeze), "the shot never came off cooldown");

        pig.setPosition(Vector3f(20.5f, 100.0f, 0.5f));
        require(!goal.canUse(arena.mOwner, breeze), "a target out of attack_range was shot at");
    }

    void trajectories() {
        Arena arena;
        const AxisAlignedBB box(0.0f, 0.0f, 0.0f, 0.6f, 1.77f, 0.6f);
        const std::optional<Vector3f> velocity = JumpAroundTargetGoal::launchVelocity(
                arena.mLevel, box, Vector3f(), Vector3f(6.0f, 0.0f, 0.0f), 45.0f, 1.4f, 0.7f, false);
        require(velocity.has_value(), "a short jump found no velocity");
        // v^2 = d^2 g / (2 sin cos d) on flat ground
        const float speed = std::sqrt(36.0f * 0.08f / 6.0f);
        require(near(velocity->x, speed * std::cos(0.7853982f), 0.01f), "the launch speed along x is off");
        require(near(velocity->y, speed * std::sin(0.7853982f), 0.01f), "the launch speed upward is off");
        require(near(velocity->z, 0.0f, 0.01f), "the jump leaves its line");
        require(!JumpAroundTargetGoal::launchVelocity(arena.mLevel, box, Vector3f(), Vector3f(40.0f, 0.0f, 0.0f), 45.0f,
                                                      1.4f, 0.7f, false)
                         .has_value(),
                "a jump faster than max_jump_velocity was allowed");
    }

    void landings() {
        Arena arena;
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(0.5f, 100.0f, 0.5f));
        pig.setRotation(Vector3f());
        std::mt19937 random(7);
        for (int32_t draw = 0; draw < 50; draw++) {
            const Vector3f spot = JumpAroundTargetGoal::positionAroundTarget(pig, 0.0f, 4.0f, 8.0f, random);
            const float dz = spot.z - 0.5f;
            require(near(spot.x, 0.5f, 0.01f), "a spot with no spread is off the line behind the target");
            // the head faces +z: behind is -z
            require(dz <= -4.0f + 0.01f && dz >= -8.0f - 0.01f, "the spot is not behind the target within its range");
        }
    }

    void jumps() {
        Arena arena;
        arena.floor(99, 14);
        MobActor &breeze = arena.spawn("minecraft:breeze", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(0.5f, 100.0f, 10.5f));
        breeze.setOnGround(true);
        require(breeze.setTarget(arena.mOwner, pig.getRuntimeId()), "the breeze could not target the pig");

        JumpAroundTargetGoal::Settings settings;
        settings.mJumpAngles = {40.0f, 55.0f, 60.0f, 75.0f, 80.0f};
        JumpAroundTargetGoal goal(settings);
        bool usable = false;
        // the landing is drawn at random: a few tries find one in the open
        for (int32_t attempt = 0; attempt < 20 && !usable; attempt++)
            usable = goal.canUse(arena.mOwner, breeze);
        require(usable, "a target in valid_distance_to_target was never jumped around");
        goal.start(arena.mOwner, breeze);
        require(breeze.getFlags().get(ActorFlag::JumpGoalJump), "the jump flag was not set");

        for (int32_t tick = 0; tick < 10; tick++) {
            goal.tick(arena.mOwner, breeze);
            require(breeze.getMotion().y == 0.0f || tick == 10, "the breeze jumped before prepare_jump_duration");
            ServerTickProbe::advance(arena.mOwner, 1);
        }
        goal.tick(arena.mOwner, breeze);
        require(breeze.getMotion().y > 0.0f, "the breeze did not leap");
        require(breeze.getCollisionScale() == 0.7f, "the box was not shrunk for the jump");
        require(breeze.discardsFriction(), "the ground friction was kept during the jump");

        breeze.setOnGround(true);
        goal.tick(arena.mOwner, breeze);
        require(!goal.canContinueToUse(arena.mOwner, breeze), "landing did not end the jump");
        goal.stop(arena.mOwner, breeze);
        require(!breeze.getFlags().get(ActorFlag::JumpGoalJump), "the jump flag stayed set");
        require(breeze.getCollisionScale() == 1.0f && !breeze.discardsFriction(), "the jump did not restore the breeze");
        require(!goal.canUse(arena.mOwner, breeze), "jumped again inside jump_cooldown_duration");

        // no room overhead: no jump
        ServerTickProbe::advance(arena.mOwner, 20);
        arena.mLevel.setBlockState(0, 102, 0, BlockState("minecraft:stone"));
        for (int32_t attempt = 0; attempt < 20; attempt++)
            require(!goal.canUse(arena.mOwner, breeze), "jumped without required_vertical_space");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-breeze-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        anchors();
        fires();
        trajectories();
        landings();
        jumps();
        std::cout << "breeze goal checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
