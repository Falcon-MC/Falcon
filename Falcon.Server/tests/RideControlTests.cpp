#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Movement/RideControlSystem.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    class TestMount final : public MobActor {
    public:
        explicit TestMount(uint64_t id) : MobActor(id, "test:mount") {}
        ActorCategory getCategory() const override {
            return ActorCategory::Passive;
        }
        ActorSize getSize() const override {
            return {1.0f, 1.0f};
        }
        float getDefaultMaxHealth() const override {
            return 20.0f;
        }
    };

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void near(float actual, float expected, const char *message) {
        require(std::isfinite(actual) && std::fabs(actual - expected) < 0.00001f, message);
    }
}

// Only component fixtures and isolated system steps require private access.
class RideControlProbe {
public:
    static void components(MobActor &mob, const json::Value &definition) {
        mob.mComponents.clear();
        for (const std::string &key: definition.mKeys)
            mob.mComponents[key] = definition.get(key);
        mob.mComponentsDirty = false;
        RideControlSystem::reset(mob);
        mob.setOnGround(true);
        mob.setMotion(Vector3f(0, 0, 0));
        mob.setRotation(Vector3f(0, 0, 0));
    }

    static void rotate(MobActor &mob, float target) {
        RideControlSystem::_applyAirRotation(mob, target);
    }

    static bool dash(ServerNetworkHandler &owner, MobActor &mob, float charge) {
        return RideControlSystem::_tryDash(owner, mob, RideControlType::Ground, charge);
    }
};

namespace {
    void checkRideControl() {
        ServerNetworkHandler owner("Ride regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer &rider = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        auto mountOwner = std::make_unique<TestMount>(1002);
        TestMount &mount = *mountOwner;
        mount._addPassenger(rider.getUniqueId());
        rider._setVehicleId(mount.getUniqueId());
        owner.getActors().emplace(mount.getUniqueId(), std::move(mountOwner));
        const auto air = json::parse(R"({"minecraft:free_camera_controlled":{"strafe_speed_modifier":1.0},"minecraft:flying_speed":{"value":0.016}})");
        RideControlProbe::components(mount, *air);
        PlayerAuthInputPacket packet;
        packet.mMotionY = 1.0f;
        packet.mRawMoveVectorY = 1.0f;
        const auto step = [&] {
            RideControlSystem::receiveInput(owner, rider, packet);
            require(RideControlSystem::tick(owner, mount), "Controller was not recognized");
        };
        step();
        const float forwardSpeed = mount.getMotion().z;
        require(forwardSpeed > 0.0f, "Forward input produced no movement");
        packet.mRawMoveVectorY = 2.0f;
        step();
        near(mount.getMotion().z, forwardSpeed, "Raw forward input escaped its bound");
        packet.mRawMoveVectorX = 2.0f;
        step();
        const RiderInput &input = mount.getRideControl().mInput;
        near(input.mRawStrafe * input.mRawStrafe + input.mRawForward * input.mRawForward,
             1.0f, "Diagonal input exceeded unit length");
        packet.mRawMoveVectorX = std::numeric_limits<float>::quiet_NaN();
        packet.mRawMoveVectorY = std::numeric_limits<float>::infinity();
        step();
        near(input.mRawStrafe, 0.0f, "Nonfinite strafe survived validation");
        near(input.mRawForward, 0.0f, "Nonfinite forward survived validation");
        packet.mInteractRotationY = std::numeric_limits<float>::max();
        packet.mInteractRotationX = std::numeric_limits<float>::max();
        step();
        require(std::fabs(input.mYaw) <= 180.0f, "Large finite yaw escaped wrapping");
        near(input.mPitch, 90.0f, "Pitch escaped its bound");
        packet.mInteractRotationY = 0.0f;
        packet.mRawMoveVectorX = 1.0f;
        packet.mRawMoveVectorY = 0.0f;
        packet.mInteractRotationX = 45.0f;
        step();
        near(mount.getMotion().y, 0.0f, "Strafing alone caused vertical travel");
        packet.mRawMoveVectorX = 0.0f;
        packet.mRawMoveVectorY = 1.0f;
        step();
        near(mount.getMotion().z, forwardSpeed * std::sqrt(0.5f), "Pitch did not project forward travel");
        require(mount.getMotion().y < 0.0f, "Looking down did not descend");
        packet.mRawMoveVectorY = -1.0f;
        step();
        require(mount.getMotion().y > 0.0f, "Backward travel did not reverse vertical direction");
        mount.setRotation(Vector3f(0, 0, 0));
        RideControlProbe::rotate(mount, 90.0f);
        near(mount.getRotation().y, 11.34f, "BDS large-angle rotation differs");
        RideControlProbe::rotate(mount, -90.0f);
        require(mount.getRotation().y < 11.34f, "Rotation ignored the latest target");
        mount.setRotation(Vector3f(0, 179.0f, 179.0f));
        RideControlProbe::rotate(mount, -179.0f);
        near(mount.getRotation().y, 179.0f + 2.0f * (43.0f / 90.0f) * 0.7f,
             "Rotation did not take the short path across 180 degrees");
        packet.mInputData = {(int32_t) PlayerAuthInputData::Sprinting,
                             (int32_t) PlayerAuthInputData::StopSprinting};
        step();
        require(!input.mSprinting, "Stop sprinting did not override sprint flags");
        require(!input.mReceived, "Control request was not consumed");
        mount.setMotion(Vector3f(0.2f, 0.3f, 0.4f));
        RideControlSystem::tick(owner, mount);
        near(mount.getMotion().z, 0.4f, "A missing request replayed old controls");

        const auto ground = json::parse(R"({"minecraft:input_ground_controlled":{},"minecraft:movement":{"value":0.2}})");
        RideControlProbe::components(mount, *ground);
        packet = PlayerAuthInputPacket();
        packet.mMotionY = 1.0f;
        step();
        const float groundForward = mount.getMotion().z;
        mount.setMotion(Vector3f(0, 0, 0));
        packet.mMotionY = 0.0f;
        packet.mMotionX = 1.0f;
        step();
        near(mount.getMotion().x, groundForward * 0.5f, "Ground strafe modifier differs from BDS");
        mount.setMotion(Vector3f(0, 0, 0));
        packet.mMotionX = 0.0f;
        packet.mMotionY = -1.0f;
        step();
        near(mount.getMotion().z, -groundForward * 0.25f, "Ground backward modifier differs from BDS");

        const auto jump = json::parse(R"({"minecraft:input_ground_controlled":{},"minecraft:can_power_jump":{},"minecraft:horse.jump_strength":{"value":0.7}})");
        RideControlProbe::components(mount, *jump);
        packet = PlayerAuthInputPacket();
        packet.mInputData = {(int32_t) PlayerAuthInputData::JumpDown};
        step();
        require(mount.getRideControl().mJumpChargeTicks == 1, "First held tick was lost");
        for (int i = 0; i < 30; ++i)
            RideControlSystem::tick(owner, mount);
        require(mount.getRideControl().mJumpChargeTicks == 1, "Missing packets increased charge");
        packet.mInputData.clear();
        step();
        require(mount.getMotion().y > 0.0f, "One-tick jump charge was discarded");
        require(mount.getRideControl().mJumpChargeTicks == -1, "Jump charge was not consumed");
        const float shortJump = mount.getMotion().y;
        packet.mInputData = {(int32_t) PlayerAuthInputData::JumpDown};
        for (int tick = 0; tick < 12; ++tick)
            step();
        packet.mInputData.clear();
        step();
        near(mount.getMotion().y / shortJump, (0.8f + 0.2f / 3.0f) / 0.1f,
             "Overheld jump charge did not decay after the tenth tick");


        const auto dash = json::parse(R"({"minecraft:input_ground_controlled":{},"minecraft:movement":{"value":0.09},"minecraft:dash_action":{"horizontal_momentum":20.0,"vertical_momentum":0.6,"cooldown_time":0.0}})");
        RideControlProbe::components(mount, *dash);
        require(RideControlProbe::dash(owner, mount, 0.5f), "Half-charge dash failed");
        const Vector3f half = mount.getMotion();
        mount.setMotion(Vector3f(0, 0, 0));
        require(RideControlProbe::dash(owner, mount, 1.0f), "Full-charge dash failed");
        const Vector3f full = mount.getMotion();
        near(half.z, full.z * 0.5f, "Horizontal dash charge was nonlinear");
        near(half.y, 0.3f, "Vertical half-charge impulse differs from BDS");
        near(full.z, 1.8f, "Dash did not use the movement attribute");
        near(full.y, 0.6f, "Dash did not use vertical momentum");
        MobEffectInstance effect;
        effect.mId = MobEffectId::JumpBoost;
        effect.mDuration = 100;
        effect.mAmplifier = 1;
        mount.addEffect(effect);
        mount.setMotion(Vector3f(0, 0, 0));
        RideControlProbe::dash(owner, mount, 1.0f);
        near(mount.getMotion().y, 0.72f, "Dash omitted Jump Boost II");
        mount.setOnGround(false);
        require(!RideControlProbe::dash(owner, mount, 1.0f), "Ground dash accepted an airborne release");
        const auto passengerDash = json::parse(R"({"minecraft:input_ground_controlled":{},"minecraft:movement":{"value":0.09},"minecraft:dash_action":{"horizontal_momentum":20.0,"vertical_momentum":0.6,"cooldown_time":2.75,"direction":"passenger"}})");
        RideControlProbe::components(mount, *passengerDash);
        mount.getRideControl().mInput.mPitch = 90.0f;
        require(RideControlProbe::dash(owner, mount, 1.0f), "Passenger dash failed");
        near(mount.getMotion().y, -1.8f, "Passenger pitch did not rotate the forward impulse");
        near(mount.getMotion().z, 0.72f, "Passenger pitch did not rotate the upward impulse");
        require(mount.getRideControl().mDashCooldownEndTick == owner.getCurrentTick() + 55,
                "Definition cooldown was not converted to ticks");
        require(!RideControlProbe::dash(owner, mount, 1.0f), "Dash ignored its cooldown");
        RideControlSystem::reset(mount);
        require(!RideControlProbe::dash(owner, mount, 1.0f), "Remount reset the dash cooldown");

    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-ride-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        checkRideControl();
    } catch (const std::exception &error) {
        std::clog << "Ride control regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
