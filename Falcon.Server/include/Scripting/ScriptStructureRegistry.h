#pragma once

#include "World/Level/Structure/McStructure.h"
#include "World/Level/Structure/PackJigsaw.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Level;

// world.structureManager: one registry for every pack; memory structures live until shutdown, world ones
// under structuretemplate_<id> in the world database, pack ones read from each pack's structures folder
class ScriptStructureRegistry {
public:
    // the world database key prefix, from the 26.60 strings
    static constexpr const char *WORLD_PREFIX = "structuretemplate_";

    // mystructure: the namespace an identifier without one takes, from the 26.60 strings
    static std::string normalize(const std::string &identifier);

    // false for an identifier the game refuses: empty, or more than one namespace separator
    static bool isValidIdentifier(const std::string &identifier);

    // structures/<name>.mcstructure is mystructure:<name>; structures/<ns>/<path>.mcstructure is <ns>:<path>
    void addPack(const std::string &packRoot);

    void setStorage(Level *storage) {
        mStorage = storage;
    }

    // memory first, then the world, then the packs; null when none has it
    std::shared_ptr<McStructure> find(const std::string &identifier);

    bool exists(const std::string &identifier);

    // false when the identifier is taken
    bool add(const std::string &identifier, std::shared_ptr<McStructure> structure, bool toWorld);

    // writes the structure under its identifier in the world database
    bool saveToWorld(const std::string &identifier);

    // memory and world copies go; pack structures stay
    bool remove(const std::string &identifier);

    bool isPackStructure(const std::string &identifier) const {
        return mPack.find(identifier) != mPack.end();
    }

    std::vector<std::string> packIds() const;

    std::vector<std::string> worldIds() const;

    // the packs' template pools, jigsaw structures and processors
    const PackJigsaw &jigsaw() const {
        return mJigsaw;
    }

    void clear() {
        mMemory.clear();
        mPack.clear();
        mWorld.clear();
        mJigsaw.clear();
    }

private:
    Level *mStorage = nullptr;
    PackJigsaw mJigsaw;
    std::map<std::string, std::shared_ptr<McStructure>> mMemory;
    std::map<std::string, std::shared_ptr<McStructure>> mWorld;
    // file paths, parsed on first use
    std::map<std::string, std::string> mPack;
    std::map<std::string, std::shared_ptr<McStructure>> mPackLoaded;
};
