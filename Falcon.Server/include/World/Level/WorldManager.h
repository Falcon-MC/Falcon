#pragma once

#include "World/Level/World.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// main thread only; ids never reused: an actor carrying an unloaded world's id resolves to the default
// world, not an unrelated one
class WorldManager {
public:
    static std::string getWorldsDirectory();

    World &add(const std::string &name, int viewDistance, int64_t seed);

    void remove(uint32_t id);

    World *find(const std::string &name);

    World *find(uint32_t id);

    World &getDefault() {
        return *mWorlds.begin()->second;
    }

    bool hasDefault() const {
        return !mWorlds.empty();
    }

    std::vector<World *> getWorlds();

    // loaded or not
    static std::vector<std::string> listOnDisk();

    static bool exists(const std::string &name);

    static bool isValidName(const std::string &name);

private:
    std::map<uint32_t, std::unique_ptr<World>> mWorlds;
    uint32_t mNextId = 0;
};
