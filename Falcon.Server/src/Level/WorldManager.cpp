#include "Level/WorldManager.h"

#include <algorithm>
#include <filesystem>

namespace {
    const size_t MAX_NAME_LENGTH = 64;
    const char *const DATABASE_FOLDER = "db";
    const char *const FORBIDDEN_CHARACTERS = "/\\:*?\"<>|";
}

const char *const WorldManager::WORLDS_DIRECTORY = "worlds";

World &WorldManager::add(const std::string &name, int viewDistance, int64_t seed) {
    const uint32_t id = mNextId++;
    std::unique_ptr<World> &slot = mWorlds[id];
    slot.reset(new World(id, name, viewDistance, seed));
    return *slot;
}

void WorldManager::remove(uint32_t id) {
    mWorlds.erase(id);
}

World *WorldManager::find(const std::string &name) {
    for (auto &entry: mWorlds) {
        if (entry.second->getName() == name)
            return entry.second.get();
    }
    return nullptr;
}

World *WorldManager::find(uint32_t id) {
    const auto found = mWorlds.find(id);
    return found == mWorlds.end() ? nullptr : found->second.get();
}

std::vector<World *> WorldManager::getWorlds() {
    std::vector<World *> worlds;
    worlds.reserve(mWorlds.size());
    for (auto &entry: mWorlds)
        worlds.push_back(entry.second.get());
    return worlds;
}

std::vector<std::string> WorldManager::listOnDisk() {
    std::vector<std::string> names;
    std::error_code error;
    for (std::filesystem::directory_iterator it(WORLDS_DIRECTORY, error), end; !error && it != end;
         it.increment(error)) {
        if (it->is_directory(error) && std::filesystem::is_directory(it->path() / DATABASE_FOLDER, error))
            names.push_back(it->path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool WorldManager::exists(const std::string &name) {
    std::error_code error;
    return isValidName(name)
           && std::filesystem::is_directory(std::filesystem::path(WORLDS_DIRECTORY) / name / DATABASE_FOLDER, error);
}

bool WorldManager::isValidName(const std::string &name) {
    if (name.empty() || name.size() > MAX_NAME_LENGTH || name == "." || name == "..")
        return false;

    if (name.find_first_of(FORBIDDEN_CHARACTERS) != std::string::npos)
        return false;

    return std::none_of(name.begin(), name.end(), [](char character) {
        return (unsigned char) character < 0x20;
    });
}
