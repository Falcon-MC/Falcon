#include "Scripting/Binding/ScriptSurface.h"

#include <cstring>
#include <unordered_map>

namespace {
#include "ScriptSurface.inc"

    const std::unordered_map<std::string, const ScriptSurfaceEntry *> &surfaceIndex() {
        static const std::unordered_map<std::string, const ScriptSurfaceEntry *> index = [] {
            std::unordered_map<std::string, const ScriptSurfaceEntry *> built;
            for (const ScriptSurfaceEntry &entry: SCRIPT_SURFACE)
                built.emplace(entry.mName, &entry);
            return built;
        }();
        return index;
    }

    const char *modulePrefix(const std::string &module) {
        if (module == "@minecraft/server-ui")
            return "ui:";
        if (module == "@minecraft/common")
            return "common:";
        return "server:";
    }
}

ScriptSurface::ScriptSurface(const std::string &module, const ScriptModuleVersion &version)
        : mPrefix(modulePrefix(module)), mTrack(version.mTrack), mMinor(version.mMinor) {}

const ScriptSurfaceEntry *ScriptSurface::_find(const std::string &name) const {
    const auto &index = surfaceIndex();
    const auto it = index.find(mPrefix + name);
    return it == index.end() ? nullptr : it->second;
}

bool ScriptSurface::has(const std::string &name) const {
    const ScriptSurfaceEntry *entry = _find(name);
    if (entry == nullptr)
        return false;

    switch (mTrack) {
        case ScriptApiTrack::Stable1:
            return entry->mFirst1 != 0xff && mMinor >= entry->mFirst1 && mMinor <= entry->mLast1;
        case ScriptApiTrack::Stable2:
            return entry->mFirst2 != 0xff && mMinor >= entry->mFirst2 && mMinor <= entry->mLast2;
        case ScriptApiTrack::Beta:
            return entry->mBeta;
    }
    return false;
}

bool ScriptSurface::isEarly(const std::string &name) const {
    const ScriptSurfaceEntry *entry = _find(name);
    return entry != nullptr && entry->mEarly;
}

bool ScriptSurface::isBlocked(const std::string &name) const {
    const ScriptSurfaceEntry *entry = _find(name);
    return entry != nullptr && entry->mBlocked;
}

std::vector<std::pair<std::string, std::string>> ScriptSurface::enumValues(const std::string &enumName) const {
    std::vector<std::pair<std::string, std::string>> values;
    const std::string prefix = mPrefix + enumName + ".";
    for (const ScriptSurfaceEntry &entry: SCRIPT_SURFACE) {
        if (std::strncmp(entry.mName, prefix.c_str(), prefix.size()) != 0)
            continue;

        const std::string member = entry.mName + prefix.size();
        const size_t equals = member.find('=');
        if (equals == std::string::npos || !has(enumName + "." + member))
            continue;
        values.emplace_back(member.substr(0, equals), member.substr(equals + 1));
    }
    return values;
}

std::vector<std::string> ScriptSurface::errorClasses() const {
    std::vector<std::string> names;
    for (const ScriptSurfaceEntry &entry: SCRIPT_SURFACE) {
        if (std::strncmp(entry.mName, mPrefix.c_str(), mPrefix.size()) != 0)
            continue;

        const std::string name = entry.mName + mPrefix.size();
        const char *parent = findParent(name);
        if (name.find('.') == std::string::npos && parent != nullptr && std::strcmp(parent, "Error") == 0 && has(name))
            names.push_back(name);
    }
    return names;
}

const ScriptSignalEntry *ScriptSurface::findSignal(const std::string &property) {
    for (const ScriptSignalEntry &entry: SCRIPT_SIGNALS) {
        if (property == entry.mProperty)
            return &entry;
    }
    return nullptr;
}

std::vector<const ScriptSignalEntry *> ScriptSurface::signalsOf(const std::string &owner) {
    std::vector<const ScriptSignalEntry *> signals;
    const std::string prefix = owner + ".";
    for (const ScriptSignalEntry &entry: SCRIPT_SIGNALS) {
        if (std::strncmp(entry.mProperty, prefix.c_str(), prefix.size()) == 0)
            signals.push_back(&entry);
    }
    return signals;
}

const char *ScriptSurface::findParent(const std::string &className) {
    for (const ScriptParentEntry &entry: SCRIPT_PARENTS) {
        if (className == entry.mClass)
            return entry.mParent;
    }
    return nullptr;
}
