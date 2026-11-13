#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/DynamicPropertyStore.h"
#include "World/Level/Level.h"
#include "World/Level/Storage/PlayerDataProvider.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
    const char *const KEY = "falcon_world_dynamic_properties";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    Tag propertiesTag(const std::string &value) {
        std::unordered_map<std::string, DynamicPropertyValue> properties;
        DynamicPropertyValue entry;
        entry.mType = DynamicPropertyValue::Type::String;
        entry.mString = value;
        properties["pack:coins"] = entry;
        Tag data = Tag::ofCompound();
        data.put("DynamicProperties", serializeDynamicProperties(properties));
        return data;
    }

    std::string coins(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("pack:coins");
        return it == owner.getWorldDynamicProperties().end() ? std::string() : it->second.mString;
    }

    // the old players/world_dynamic_properties.dat moves into the world database, then saves go there
    void migrates(const std::filesystem::path &directory) {
        ServerPaths::setRoot(directory);
        PlayerDataProvider players(ServerPaths::file("players"));
        require(players.saveData("world_dynamic_properties", propertiesTag("42")), "the legacy file was not written");

        ServerNetworkHandler owner("World dynamic properties", "", 1);
        Level &overworld = owner.getWorlds().getDefault().getOverworld();
        require(overworld.openStorage((directory / "worlds").string()), "the test world database did not open");
        owner.loadWorldDynamicProperties();
        require(coins(owner) == "42", "the legacy properties did not load");
        require(!players.hasData("world_dynamic_properties"), "the legacy file stayed among the players");

        owner.getWorldDynamicProperties()["pack:coins"].mString = "43";
        owner.saveWorldDynamicProperties(true);
        require(!players.hasData("world_dynamic_properties"), "a save wrote into the players directory");
        Tag saved;
        require(overworld.loadSavedData(KEY, saved, false), "the properties are not in the world database");
        owner.getWorldDynamicProperties().clear();
        owner.loadWorldDynamicProperties();
        require(coins(owner) == "43", "the saved properties did not load back");
    }

    // a real player save under that name is not taken for the world's properties
    void ignoresPlayerSave(const std::filesystem::path &directory) {
        ServerPaths::setRoot(directory);
        PlayerDataProvider players(ServerPaths::file("players"));
        Tag player = propertiesTag("7");
        player.putInt("PlayerGameMode", 0);
        require(players.saveData("world_dynamic_properties", player), "the player file was not written");

        ServerNetworkHandler owner("World dynamic properties", "", 1);
        Level &overworld = owner.getWorlds().getDefault().getOverworld();
        require(overworld.openStorage((directory / "worlds").string()), "the test world database did not open");
        owner.loadWorldDynamicProperties();
        require(coins(owner).empty(), "a player save was read as the world's properties");
        require(players.hasData("world_dynamic_properties"), "a player save was removed");
    }

    // a record that does not read back is kept: neither the scoreboard nor the properties save over it
    void keepsUnreadable(const std::filesystem::path &directory) {
        ServerPaths::setRoot(directory);
        ServerNetworkHandler owner("World dynamic properties", "", 1);
        Level &overworld = owner.getWorlds().getDefault().getOverworld();
        require(overworld.openStorage((directory / "worlds").string()), "the test world database did not open");
        const Tag notCompound = Tag::ofList(Tag::Type::Compound);
        require(overworld.saveSavedData(KEY, notCompound, false)
                        && overworld.saveSavedData("scoreboard", notCompound, false),
                "the unreadable records were not written");

        owner.loadWorldDynamicProperties();
        owner.loadScoreboard();
        owner.getWorldDynamicProperties()["pack:coins"].mString = "1";
        owner.saveWorldDynamicProperties(true);
        owner.saveScoreboard(true);

        Tag read;
        require(!overworld.loadSavedData(KEY, read, false) && overworld.hasSavedData(KEY),
                "unreadable world dynamic properties were saved over");
        require(!overworld.loadSavedData("scoreboard", read, false) && overworld.hasSavedData("scoreboard"),
                "an unreadable scoreboard was saved over");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto base = std::filesystem::temp_directory_path()
                      / ("falcon-world-properties-"
                         + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(base / "a");
        std::filesystem::create_directories(base / "b");
        std::filesystem::create_directories(base / "c");
        migrates(base / "a");
        ignoresPlayerSave(base / "b");
        keepsUnreadable(base / "c");
    } catch (const std::exception &error) {
        std::clog << "World dynamic properties regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);
    return result;
}
