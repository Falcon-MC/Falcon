#include "Scripting/ScriptStructureRegistry.h"

#include "World/Level/Level.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

std::string ScriptStructureRegistry::normalize(const std::string &identifier) {
    return identifier.find(':') == std::string::npos ? "mystructure:" + identifier : identifier;
}

bool ScriptStructureRegistry::isValidIdentifier(const std::string &identifier) {
    const size_t colon = identifier.find(':');
    if (identifier.empty() || colon == 0 || colon + 1 == identifier.size())
        return false;
    return colon == std::string::npos || identifier.find(':', colon + 1) == std::string::npos;
}

void ScriptStructureRegistry::addPack(const std::string &packRoot) {
    namespace fs = std::filesystem;
    mJigsaw.addPack(packRoot);
    const fs::path root = fs::path(packRoot) / "structures";
    std::error_code error;
    if (!fs::is_directory(root, error))
        return;
    for (fs::recursive_directory_iterator it(root, error), end; it != end && !error; it.increment(error)) {
        if (!it->is_regular_file(error) || it->path().extension() != ".mcstructure")
            continue;
        const fs::path relative = fs::relative(it->path(), root, error);
        std::vector<std::string> parts;
        for (const fs::path &part: relative.parent_path())
            parts.push_back(part.string());
        const std::string name = relative.stem().string();
        std::string identifier;
        if (parts.empty()) {
            identifier = "mystructure:" + name;
        } else {
            identifier = parts.front() + ":";
            for (size_t index = 1; index < parts.size(); ++index)
                identifier += parts[index] + "/";
            identifier += name;
        }
        mPack.emplace(identifier, it->path().string());
    }
}

std::shared_ptr<McStructure> ScriptStructureRegistry::find(const std::string &identifier) {
    const std::string id = normalize(identifier);
    const auto memory = mMemory.find(id);
    if (memory != mMemory.end())
        return memory->second;

    const auto world = mWorld.find(id);
    if (world != mWorld.end())
        return world->second;
    if (mStorage != nullptr) {
        Tag saved;
        McStructure structure;
        if (mStorage->loadSavedData(WORLD_PREFIX + id, saved, false) && McStructure::fromNbt(saved, structure)) {
            std::shared_ptr<McStructure> loaded = std::make_shared<McStructure>(std::move(structure));
            mWorld[id] = loaded;
            return loaded;
        }
    }

    const auto cached = mPackLoaded.find(id);
    if (cached != mPackLoaded.end())
        return cached->second;
    const auto pack = mPack.find(id);
    if (pack == mPack.end())
        return nullptr;
    std::ifstream file(pack->second, std::ios::binary);
    std::stringstream bytes;
    bytes << file.rdbuf();
    McStructure structure;
    if (!McStructure::parse(bytes.str(), structure))
        return nullptr;
    std::shared_ptr<McStructure> loaded = std::make_shared<McStructure>(std::move(structure));
    mPackLoaded[id] = loaded;
    return loaded;
}

bool ScriptStructureRegistry::exists(const std::string &identifier) {
    return find(identifier) != nullptr;
}

bool ScriptStructureRegistry::add(const std::string &identifier, std::shared_ptr<McStructure> structure, bool toWorld) {
    const std::string id = normalize(identifier);
    if (exists(id))
        return false;
    if (toWorld) {
        mWorld[id] = structure;
        saveToWorld(id);
    } else {
        mMemory[id] = structure;
    }
    return true;
}

bool ScriptStructureRegistry::saveToWorld(const std::string &identifier) {
    const std::string id = normalize(identifier);
    std::shared_ptr<McStructure> structure = find(id);
    if (structure == nullptr)
        return false;
    mWorld[id] = structure;
    mMemory.erase(id);
    return mStorage != nullptr && mStorage->saveSavedData(WORLD_PREFIX + id, structure->toNbt(), false);
}

bool ScriptStructureRegistry::remove(const std::string &identifier) {
    const std::string id = normalize(identifier);
    const bool inMemory = mMemory.erase(id) > 0;
    bool inWorld = mWorld.erase(id) > 0;
    if (mStorage != nullptr) {
        Tag saved;
        inWorld = mStorage->loadSavedData(WORLD_PREFIX + id, saved, false) || inWorld;
        if (inWorld)
            mStorage->eraseSavedData(WORLD_PREFIX + id);
    }
    return inMemory || inWorld;
}

std::vector<std::string> ScriptStructureRegistry::packIds() const {
    std::vector<std::string> ids;
    for (const auto &entry: mPack)
        ids.push_back(entry.first);
    return ids;
}

std::vector<std::string> ScriptStructureRegistry::worldIds() const {
    std::vector<std::string> ids;
    for (const auto &entry: mWorld)
        ids.push_back(entry.first);
    if (mStorage != nullptr) {
        for (const auto &entry: mStorage->loadSavedDataWithPrefix(WORLD_PREFIX)) {
            if (mWorld.find(entry.first) == mWorld.end())
                ids.push_back(entry.first);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}
