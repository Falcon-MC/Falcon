#pragma once

#include "Level/World.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * Owns every loaded World. Ids are never reused, so an actor that still carries the id of an unloaded world
 * resolves to the default world instead of to an unrelated one. Main thread only.
 */
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

    /** Folders under `worlds/` that hold a world database, loaded or not. */
    static std::vector<std::string> listOnDisk();

    static bool exists(const std::string &name);

    /** Rejects names that would escape the worlds folder or that the file system cannot hold. */
    static bool isValidName(const std::string &name);

private:
    std::map<uint32_t, std::unique_ptr<World>> mWorlds;
    uint32_t mNextId = 0;
};
