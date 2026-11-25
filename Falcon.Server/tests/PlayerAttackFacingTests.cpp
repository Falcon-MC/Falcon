#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a player standing in a pig's box, eye above it, at its back edge
    void attackFromInsideBox() {
        ServerNetworkHandler owner("Facing", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);

        auto *pig = dynamic_cast<MobActor *>(owner.spawnActor(level, "minecraft:pig", Vector3f(0.5f, 64.0f, 0.5f)));
        require(pig != nullptr, "pig did not spawn");

        const NetworkIdentifier id("facing", 100);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 100).first->second;
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setPosition(Vector3f(0.5f, 64.0f, 0.1f));
        player.setLoginState(ServerPlayer::LoginState::Spawned);

        const float health = pig->getHealth();
        // looking straight ahead passes over the pig
        player.setRotation(Vector3f(0.0f, 0.0f, 0.0f));
        require(!player.attackActor(owner, pig->getRuntimeId(), nullptr, true), "hit a pig the player looks over");
        require(pig->getHealth() == health, "pig hurt by a missed look");

        // 45 degrees down hits the pig's top, though its nearest point is straight below
        player.setRotation(Vector3f(45.0f, 0.0f, 0.0f));
        require(player.attackActor(owner, pig->getRuntimeId(), nullptr, true), "looked at pig not attacked");
        require(pig->getHealth() < health, "looked at pig not hurt");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-facing-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        attackFromInsideBox();
        std::cout << "player attack facing checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
