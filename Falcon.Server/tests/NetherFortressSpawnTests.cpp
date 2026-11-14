#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Spawn/NaturalSpawner.h"
#include "World/Actor/Spawn/SpawnRules.h"
#include "World/Actor/Spawn/StructureSpawnOverrides.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/ChunkStructureBounds.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const std::set<std::string> FORTRESS_MOBS = {"minecraft:zombie_pigman", "minecraft:magma_cube", "minecraft:blaze",
                                                 "minecraft:skeleton", "minecraft:wither_skeleton"};

    bool contains(const std::vector<std::string> &values, const char *value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    void checkTable() {
        const std::vector<SpawnRule> *rules = StructureSpawnOverrides::forStructure("minecraft:fortress", "monster");
        require(rules != nullptr && rules->size() == 5, "the fortress has no five monster entries");
        int32_t total = 0;
        for (const SpawnRule &rule: *rules) {
            require(FORTRESS_MOBS.count(rule.mIdentifier) == 1, "an unexpected fortress mob: " + rule.mIdentifier);
            require(rule.mPopulation == "monster" && rule.mConditions.size() == 1, "a fortress entry is malformed");
            const SpawnCondition &condition = rule.mConditions[0];
            require(condition.mHerds.size() == 1 && condition.mHerds[0].mMinSize == 2
                            && condition.mHerds[0].mMaxSize == 3,
                    "a fortress herd is not 2 to 3");
            total += condition.mWeight;
            if (rule.mIdentifier == "minecraft:blaze")
                require(condition.mWeight == 10, "the blaze weight differs");
            if (rule.mIdentifier == "minecraft:wither_skeleton")
                require(condition.mWeight == 8, "the wither skeleton weight differs");
        }
        require(total == 28, "the fortress weights do not add up to 28");
        require(StructureSpawnOverrides::forStructure("minecraft:fortress", "animal") == nullptr,
                "the fortress replaces another population");
        require(StructureSpawnOverrides::forStructure("minecraft:village", "monster") == nullptr,
                "a structure without an override has one");
    }

    void checkCandidates(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Fortress spawns", "", 1);
        const std::filesystem::path path = directory / "normal.properties";
        std::ofstream(path) << "difficulty=normal\n";
        owner.setProperties(PropertiesSettings(path.string()));
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Nether);
        for (int32_t x = -2; x <= 2; ++x) {
            for (int32_t z = -2; z <= 2; ++z)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }

        // a dark brick hall over the whole chunk, a fortress piece over its west half
        for (int32_t x = 0; x <= 15; ++x) {
            for (int32_t z = 0; z <= 15; ++z) {
                level.setBlockState(x, 60, z, BlockState("minecraft:nether_brick"));
                level.setBlockState(x, 64, z, BlockState("minecraft:nether_brick"));
            }
        }
        ChunkStructureBounds::Box whole{0, 50, 0, 15, 80, 15};
        ChunkStructureBounds::Box piece{0, 58, 0, 7, 70, 15};
        level.getChunk(0, 0).getStructureBounds().addStart("minecraft:fortress", 0, whole, {piece},
                                                           ChunkStructureBounds::chunkBox(0, 0, 0, 127));

        NaturalSpawner spawner;
        const std::vector<Vector3f> players = {Vector3f(3.5f, 61.0f, 40.5f)};
        const std::vector<std::string> inside = spawner.candidatesAt(level, Vector3i(3, 61, 3), false, 2, players);
        require(contains(inside, "minecraft:blaze") && contains(inside, "minecraft:wither_skeleton"),
                "no blaze or wither skeleton can spawn in the fortress piece");
        for (const std::string &identifier: inside) {
            if (SpawnRules::getPopulation(identifier) == "monster")
                require(FORTRESS_MOBS.count(identifier) == 1,
                        "a monster outside the fortress list can spawn in its piece: " + identifier);
        }

        const std::vector<std::string> outside = spawner.candidatesAt(level, Vector3i(12, 61, 3), false, 2, players);
        require(!contains(outside, "minecraft:blaze") && !contains(outside, "minecraft:wither_skeleton"),
                "a fortress mob spawns outside its pieces");

        const std::vector<std::string> peaceful = spawner.candidatesAt(level, Vector3i(3, 61, 3), false, 0, players);
        require(!contains(peaceful, "minecraft:blaze"), "a blaze spawns on peaceful");

    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-fortress-spawns-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        SpawnRules::initialize();
        checkTable();
        checkCandidates(directory);
        std::cout << "fortress spawn checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
