#pragma once

#include "Scripting/ScriptModules.h"

#include <string>
#include <vector>

struct BehaviorPack {
    std::string mName;
    std::string mUuid;
    std::string mPath;
    std::string mScriptEntry;
    std::string mScriptModuleUuid;
    std::vector<ScriptModuleDependency> mScriptModules;
    std::vector<std::string> mDependencyUuids;

    bool hasScript() const { return !mScriptEntry.empty(); }

    std::string scriptPath() const { return mPath + "/" + mScriptEntry; }
};

class BehaviorPackManager {
public:
    void discover(const std::string &directory);

    const std::vector<BehaviorPack> &getPacks() const { return mPacks; }

    size_t getPackCount() const { return mPacks.size(); }

    std::vector<std::string> getLoadedUuids() const;

    size_t pruneUnsatisfied(const std::vector<std::string> &availableUuids);

private:
    static bool _readManifest(const std::string &manifest, BehaviorPack &out);

    void _scanRoot(const std::string &root);

    void _scanContainer(const std::string &directory, const std::string &cacheDirectory);

    void _addPackFromManifest(const std::string &packDirectory);

    std::vector<BehaviorPack> mPacks;
};
