#pragma once

#include "Scripting/ScriptModules.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct ScriptSurfaceEntry {
    const char *mName;
    uint8_t mFirst1;
    uint8_t mLast1;
    uint8_t mFirst2;
    uint8_t mLast2;
    bool mBeta;
    bool mEarly;
    bool mBlocked;
};

struct ScriptSignalEntry {
    const char *mProperty;
    const char *mSignalClass;
    const char *mEventClass;
};

struct ScriptParentEntry {
    const char *mClass;
    const char *mParent;
};

// what one module version declares, from the official definitions; names are "Class", "Class.member" and
// "Enum.Key=value"
class ScriptSurface {
public:
    ScriptSurface(const std::string &module, const ScriptModuleVersion &version);

    bool has(const std::string &name) const;

    // a method callable, or a property readable, in early execution
    bool isEarly(const std::string &name) const;

    // a method, or a property setter, refused in restricted execution
    bool isBlocked(const std::string &name) const;

    std::vector<std::pair<std::string, std::string>> enumValues(const std::string &enumName) const;

    // every Error subclass the version exports
    std::vector<std::string> errorClasses() const;

    static const ScriptSignalEntry *findSignal(const std::string &property);

    static const char *findParent(const std::string &className);

private:
    const ScriptSurfaceEntry *_find(const std::string &name) const;

    std::string mPrefix;
    ScriptApiTrack mTrack;
    int mMinor;
};
