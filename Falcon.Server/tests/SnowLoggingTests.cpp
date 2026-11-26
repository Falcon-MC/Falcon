#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/BlockPrecipitation.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
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

    void placeSnow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, float clickX) {
        ItemUseTransaction use;
        use.mBlockFace = 1;
        use.mHotbarSlot = player.getInventory().getSelectedSlot();
        use.mItemInHand = player.getInventory().getItemInHand();
        use.mBlockPosition = position;
        use.mPlayerPosition = player.getPosition();
        use.mClickPosition = Vector3f(clickX, 0.5f, 0.5f);
        use.mClientInteractPrediction = ItemUsePredictedResult::Success;
        require(BlockActionHandler::placeBlock(owner, player, use), "snow placement was not handled");
    }

    void run(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Snow logging", "", 1);
        const auto properties = directory / "test.properties";
        std::ofstream(properties) << "spawn-protection=0\n";
        owner.setProperties(PropertiesSettings(properties.string()));
        owner.getScriptEngine().bindHost(owner);
        owner.getScriptEngine().startup();
        EntityDefinitions::initialize();

        Level &level = owner.getWorlds().getDefault().getOverworld();
        // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins the insert
        level.dropChunk(0, 0);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        const Vector3i plant(4, 100, 4);
        level.setBlockState(plant.x, plant.y - 1, plant.z, BlockState("minecraft:dirt"));
        level.setBlockState(plant.x, plant.y, plant.z, BlockState("minecraft:dandelion"));
        require(BlockPrecipitation::isSnowloggable(level.getBlockState(plant.x, plant.y, plant.z)),
                "the fixture plant is not snowloggable");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setName("Tester");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(4.5f, 100.0f, 3.4f));
        player.setGameType((int32_t) GameType::Creative);
        player.getInventoryManager().attach(&player, &owner);
        player.getInventory().setSelectedSlot(0);
        player.getInventory().setItem(0, owner.createItemStack("minecraft:snow_layer", 1));

        for (int32_t height = 0; height <= 7; ++height) {
            placeSnow(owner, player, plant, 0.1f + (float) height * 0.1f);
            const BlockState base = level.getBlockState(plant.x, plant.y, plant.z);
            const BlockState overlay = level.getBlockStateAtLayer(plant.x, plant.y, plant.z, 1);
            require(base.mName == "minecraft:snow_layer", "snow was not stored in layer 0");
            require(overlay.mName == "minecraft:dandelion", "snow replaced the snowloggable plant");
            require(base.mStates.getInt("height", 0) == height,
                    "snow layer height did not grow to " + std::to_string(height));
        }

        placeSnow(owner, player, plant, 0.95f);
        const BlockState fullSnow = level.getBlockState(plant.x, plant.y, plant.z);
        require(fullSnow.mName == "minecraft:snow_layer" && fullSnow.mStates.getInt("height", 0) == 7,
                "a full snow layer grew past its maximum height");
        require(level.getBlockStateAtLayer(plant.x, plant.y, plant.z, 1).mName == "minecraft:dandelion",
                "a full snow layer removed the plant overlay");
        require(level.getBlockState(plant.x, plant.y + 1, plant.z).mName == "minecraft:snow_layer",
                "a new snow layer did not stack above a full snow layer");

        const Vector3i stone(8, 100, 8);
        level.setBlockState(stone.x, stone.y, stone.z, BlockState("minecraft:stone"));
        player.setPosition(Vector3f(8.5f, 100.0f, 7.4f));
        placeSnow(owner, player, stone, 0.35f);
        const BlockState ordinarySnow = level.getBlockState(stone.x, stone.y + 1, stone.z);
        require(ordinarySnow.mName == "minecraft:snow_layer", "snow placement on a solid block changed");
        require(level.getBlockState(stone.x, stone.y, stone.z).mName == "minecraft:stone",
                "ordinary snow placement changed its support block");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-snow-logging-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Snow logging regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
