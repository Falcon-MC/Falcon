#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"
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

    bool near(float value, float expected) {
        return std::fabs(value - expected) < 0.001f;
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Resistance", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            mLevel->insertChunk(LevelChunk(0, 0)).setPopulated(true);
        }

        // resistance I: a fifth of the damage taken off
        ServerPlayer &resistant(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            mOwner.addEffect(player, MobEffectId::Resistance, 0, 1000);
            return player;
        }
    };

    void hitOnce(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &player = arena.resistant(9001);
        const float health = player.getHealth();
        arena.mOwner.hurt(player, 10.0f, ActorDamageSource::environment("death.attack.generic", player.getName()));
        require(near(player.getHealth(), health - 8.0f), "resistance I did not take exactly a fifth of a hit");
    }

    void voidIgnoresResistance(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &player = arena.resistant(9002);
        const float health = player.getHealth();
        arena.mOwner.hurt(player, 10.0f, ActorDamageSource::environment("death.attack.outOfWorld", player.getName()));
        require(near(player.getHealth(), health - 10.0f), "resistance softened the void");
    }

    void effectDamageOnce(const std::filesystem::path &directory) {
        Arena arena(directory);
        ServerPlayer &player = arena.resistant(9003);
        const float health = player.getHealth();
        MobEffectInstance harming;
        harming.mId = MobEffectId::InstantDamage;
        InstantDamageMobEffect().tick(player, harming);
        require(near(player.getHealth(), health - 4.8f), "resistance I did not take a fifth of instant damage");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-resistance-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        hitOnce(directory);
        voidIgnoresResistance(directory);
        effectDamageOnce(directory);
        std::cout << "resistance checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
