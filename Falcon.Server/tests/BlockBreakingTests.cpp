#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    PropertiesSettings settings(const std::filesystem::path &directory, const std::string &contents) {
        const auto path = directory / "test.properties";
        std::ofstream(path) << contents;
        return PropertiesSettings(path.string());
    }

    void checkSettings(const std::filesystem::path &directory) {
        PropertiesSettings defaults;
        require(!defaults.getServerAuthoritativeBlockBreaking(), "Authoritative mining default differs");
        require(defaults.getServerAuthoritativeBlockBreakingRangeScalar() == 1.5f, "Range default differs");
        const std::string generated = PropertiesSettings::getDefaultContents();
        require(generated.find("server-authoritative-block-breaking=false\n") != std::string::npos,
                "Generated properties omit authoritative mining");
        require(generated.find("server-authoritative-block-breaking-range-scalar=1.5\n") != std::string::npos,
                "Generated properties omit the range scalar");
        for (const std::string value: {"1", "0.9", "-2", "nan", "inf", "1e1000", "bad"}) {
            auto invalid = settings(directory, "server-authoritative-block-breaking-range-scalar=" + value);
            require(invalid.getServerAuthoritativeBlockBreakingRangeScalar() == 1.5f,
                    "Invalid range did not fall back to the default");
            require(invalid.getInvalidProperties().size() == 1, "Invalid range was not reported");
        }
        PropertiesSettings generatedFile((directory / "server.properties").string());
        require(generatedFile.isLoaded() && generatedFile.hasProperty("server-authoritative-block-breaking")
                && generatedFile.hasProperty("server-authoritative-block-breaking-range-scalar"),
                "New server.properties did not receive both settings");
        auto invalidBool = settings(directory, "server-authoritative-block-breaking=1\n");
        require(!invalidBool.getServerAuthoritativeBlockBreaking()
                && invalidBool.getInvalidProperties().size() == 1, "Nonboolean mining flag was accepted");
        auto alias = settings(directory, "server-authoritative-block-breaking-pick-range-scalar=2.0\n");
        require(alias.getServerAuthoritativeBlockBreakingRangeScalar() == 2.0f, "BDS spelling was ignored");
        auto both = settings(directory, "server-authoritative-block-breaking-range-scalar=1.75\n"
                                        "server-authoritative-block-breaking-pick-range-scalar=2.0\n");
        require(both.getServerAuthoritativeBlockBreakingRangeScalar() == 1.75f, "Alias precedence differs");
    }

    void checkMining(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Block regression", "", 1);
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        player.setPosition(Vector3f(0.5f, 250.0f, 0.5f));
        player.setRotation(Vector3f(0, 0, 0));
        const Vector3i outsideBase(0, 251, 8);
        const Vector3i outsideScaled(0, 251, 11);
        const Vector3i block(0, 251, 2);
        auto legacy = settings(directory, "server-authoritative-block-breaking=false\nspawn-protection=0\n");
        owner.setProperties(legacy);
        require(!BlockActionHandler::canBreakBlock(owner, player, outsideBase), "Legacy reach was extended");
        auto authoritative = settings(directory, "server-authoritative-block-breaking=true\n"
                                                "server-authoritative-block-breaking-range-scalar=1.5\n"
                                                "spawn-protection=0\n");
        owner.setProperties(authoritative);
        require(BlockActionHandler::canBreakBlock(owner, player, outsideBase), "Mining range scalar was ignored");
        require(!BlockActionHandler::canBreakBlock(owner, player, outsideScaled), "Scaled mining reach exceeded its bound");
        require(!BlockActionHandler::canInteractWithBlock(player, outsideBase), "Mining scalar extended placement reach");
        player.setPosition(Vector3f(std::numeric_limits<float>::quiet_NaN(), 250, 0.5f));
        require(!BlockActionHandler::canBreakBlock(owner, player, block), "Nonfinite position passed reach validation");
        player.setPosition(Vector3f(0.5f, 250.0f, 0.5f));
        // setProperties reloads worlds, so no Level reference may survive a settings change.
        const auto level = [&]() -> Level & {
            return owner.getLevelFor(player);
        };
        level().setBlockState(block.x, block.y, block.z, BlockState("minecraft:slime"));
        player.startBreakingBlock(block, 0, 1.0, owner.getCurrentTick() - 1);
        BlockActionHandler::continueBreakingBlock(owner, player);
        require(level().getBlockState(block.x, block.y, block.z).mName == "minecraft:air",
                "Authoritative tick did not complete ready mining");
        owner.setProperties(legacy);
        level().setBlockState(block.x, block.y, block.z, BlockState("minecraft:slime"));
        player.startBreakingBlock(block, 0, 1.0, owner.getCurrentTick() - 1);
        BlockActionHandler::continueBreakingBlock(owner, player);
        require(level().getBlockState(block.x, block.y, block.z).mName == "minecraft:slime",
                "Legacy mode completed mining before client confirmation");
        BlockActionHandler::completeBreakingBlock(owner, player, block);
        require(level().getBlockState(block.x, block.y, block.z).mName == "minecraft:air",
                "Legacy mode rejected ready client confirmation");
        for (const auto &config: {legacy, authoritative}) {
            owner.setProperties(config);
            level().setBlockState(block.x, block.y, block.z, BlockState("minecraft:stone"));
            player.startBreakingBlock(block, 0, 0.001, owner.getCurrentTick());
            BlockActionHandler::completeBreakingBlock(owner, player, block);
            require(level().getBlockState(block.x, block.y, block.z).mName == "minecraft:stone",
                    "Configuration bypassed the minimum mining time");
            player.stopBreakingBlock();
            BlockActionHandler::completeBreakingBlock(owner, player, block);
            require(level().getBlockState(block.x, block.y, block.z).mName == "minecraft:stone",
                    "Unstarted mining was accepted");
        }
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-block-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        checkSettings(directory);
        checkMining(directory);
    } catch (const std::exception &error) {
        std::clog << "Block breaking regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
