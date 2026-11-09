#include "Core/NBT/Tag.h"
#include "Server/BanList.h"
#include "Server/OpList.h"
#include "World/Level/Storage/PlayerDataProvider.h"

#include <cstdio>
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

    std::filesystem::path scratch() {
        const std::filesystem::path directory = std::filesystem::temp_directory_path() / "falcon-account-binding";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        return directory;
    }

    void operatorsFollowTheAccount(const std::filesystem::path &directory) {
        const std::string path = (directory / "ops.txt").string();
        {
            std::ofstream file(path);
            file << "Steve\n";
        }

        OpList ops(path);
        require(ops.isOp("steve"), "a listed name is not an operator");
        require(ops.isOp("Steve", "1001"), "the first account under an operator name was refused");

        ops.bindXuid("Steve", "1001");
        require(ops.isOp("Steve", "1001"), "the recorded account lost its rank");
        require(!ops.isOp("Steve", "2002"), "another account took an operator name over");
        require(ops.isOp("Steve", ""), "an unsigned login has no account to compare");

        ops.bindXuid("Steve", "2002");
        require(!ops.isOp("Steve", "2002"), "a recorded account was replaced");

        OpList reloaded(path);
        require(reloaded.isOp("Steve", "1001") && !reloaded.isOp("Steve", "2002"),
                "the recorded account did not survive a reload");
        require(reloaded.getNames().size() == 1 && reloaded.getNames()[0] == "steve",
                "the account leaked into the operator names");
    }

    void bansFollowTheAccount(const std::filesystem::path &directory) {
        const std::string path = (directory / "banned-players.json").string();
        BanList bans(path);
        bans.add("Griefer", "grief", "console");
        require(bans.find("griefer") != nullptr, "a banned name is not found");
        require(bans.find("Renamed", "3003") == nullptr, "an unrelated account is banned");

        bans.bindXuid("Griefer", "3003");
        require(bans.find("Renamed", "3003") != nullptr, "a renamed account escaped its ban");

        BanList reloaded(path);
        require(reloaded.find("Renamed", "3003") != nullptr, "the recorded account did not survive a reload");
        require(reloaded.remove("Griefer") && reloaded.find("Renamed", "3003") == nullptr,
                "a pardon left the account banned");
    }

    void playerNamesNeverLeaveTheDirectory(const std::filesystem::path &directory) {
        const std::filesystem::path players = directory / "players";
        PlayerDataProvider provider(players.string());

        Tag data = Tag::ofCompound();
        data.putInt("Marker", 7);

        require(!PlayerDataProvider::isSafeName("../escape"), "a traversal passed as a name");
        require(!PlayerDataProvider::isSafeName("a/b") && !PlayerDataProvider::isSafeName("a\\b")
                && !PlayerDataProvider::isSafeName("a:b") && !PlayerDataProvider::isSafeName(""),
                "a path character passed as a name");
        require(!provider.saveData("../escape", data), "a traversal was saved");
        require(!std::filesystem::exists(directory / "escape.dat"), "a file was written outside the directory");

        Tag loaded;
        require(!provider.loadData("../escape", loaded) && !provider.hasData("../escape"),
                "a traversal was read");

        require(provider.saveData("Alex Smith", data), "an ordinary name was refused");
        require(provider.loadData("alex smith", loaded) && loaded.getInt("Marker") == 7,
                "an ordinary name did not load back");

        require(provider.archiveData("Alex Smith", "4004"), "a save could not be set aside");
        require(!provider.hasData("Alex Smith") && std::filesystem::exists(players / "alex smith.4004.dat"),
                "the archived save is not where it should be");
    }
}

int main() {
    try {
        const std::filesystem::path directory = scratch();
        operatorsFollowTheAccount(directory);
        bansFollowTheAccount(directory);
        playerNamesNeverLeaveTheDirectory(directory);
        std::filesystem::remove_all(directory);
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
