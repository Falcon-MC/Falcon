#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileComponent.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool near(float value, float expected, float tolerance) {
        return std::fabs(value - expected) <= tolerance;
    }

    float lengthOf(const Vector3f &motion) {
        return std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z);
    }

    // entity_definitions.json: the values the component reads
    void definitions() {
        ProjectileComponent potion;
        require(ProjectileComponent::find("minecraft:splash_potion", nullptr, potion), "no splash potion definition");
        require(potion.mPower == 0.5f && potion.mAngleOffset == -20.0f && potion.mGravity == 0.05f,
                "the splash potion did not read power 0.5, angle_offset -20, gravity 0.05");

        ProjectileComponent arrow;
        require(ProjectileComponent::find("minecraft:arrow", nullptr, arrow), "no arrow definition");
        require(arrow.mUncertaintyBase == 16.0f && arrow.mUncertaintyMultiplier == 4.0f,
                "the arrow did not read uncertainty_base 16, uncertainty_multiplier 4");
    }

    // a player looking straight ahead: speed, pitch and group come from the definitions
    void launches() {
        ServerNetworkHandler owner("ProjectileLaunch", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 3003).first->second;
        player.setName("Thrower");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        player.setRotation(Vector3f(0.0f, 0.0f, 0.0f));

        ServerActor *snowball = owner.spawnProjectile(player, "minecraft:snowball");
        require(snowball != nullptr, "no snowball");
        const Vector3f thrown = snowball->getMotion();
        require(near(thrown.z, 1.5f, 1.0e-4f) && near(thrown.y, 0.0f, 1.0e-4f), "the snowball left at not power 1.5");
        require(snowball->getProjectileComponent().mGravity == 0.03f, "the snowball does not fall at gravity 0.03");

        // a multishot side shot: turned 10 degrees about the vertical, and already facing that way at spawn
        ServerActor *side = owner.spawnProjectile(player, "minecraft:snowball", 1.0f, 0.0f, nullptr, 10.0f);
        require(side != nullptr, "no turned snowball");
        const float turn = 10.0f * 3.14159265f / 180.0f;
        const Vector3f turned = side->getMotion();
        require(near(turned.x, -1.5f * std::sin(turn), 1.0e-4f) && near(turned.z, 1.5f * std::cos(turn), 1.0e-4f),
                "the side shot was not turned 10 degrees");
        require(near(side->getRotation().y, -10.0f, 1.0e-3f), "the side shot did not face its turned flight");

        // power 0.5 aimed 20 degrees above the look
        ServerActor *bottle = owner.spawnProjectile(player, "minecraft:xp_bottle");
        require(bottle != nullptr, "no experience bottle");
        const Vector3f lobbed = bottle->getMotion();
        require(near(lengthOf(lobbed), 0.5f, 1.0e-4f), "the experience bottle left at not power 0.5");
        require(near(lobbed.y, 0.5f * std::sin(20.0f * 3.14159265f / 180.0f), 1.0e-4f),
                "the experience bottle was not aimed 20 degrees up");

        // the spawn event gives a player's arrow player_arrow: power 3, uncertainty 1
        ServerActor *arrow = owner.spawnProjectile(player, "minecraft:arrow");
        require(arrow != nullptr, "no arrow");
        require(arrow->getProjectileComponent().mPower == 3.0f
                        && arrow->getProjectileComponent().mUncertaintyBase == 1.0f,
                "the player's arrow did not take player_arrow");
        require(near(lengthOf(arrow->getMotion()), 3.0f, 0.15f), "the arrow left at not power 3");

        // held crossbow: player_crossbow_arrow, power 3.15
        player.getInventory().setItem(0, owner.createItemStack("minecraft:crossbow", 1));
        player.getInventory().setSelectedSlot(0);
        ServerActor *bolt = owner.spawnProjectile(player, "minecraft:arrow");
        require(bolt != nullptr && bolt->getProjectileComponent().mPower == 3.15f,
                "the crossbow arrow did not take player_crossbow_arrow");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-projectile-launch-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        definitions();
        launches();
        std::cout << "projectile launch checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Projectile launch regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
