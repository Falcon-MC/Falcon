#include "Plugin/InternalPluginLoader.h"

#include "Actor/ActorClassRegistry.h"
#include "Block/BlockActorClassRegistry.h"
#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Debug/BedrockLog.h"
#include "Item/ItemClassRegistry.h"
#include "Item/VanillaItems.h"
#include "Plugin/InternalPluginBuild.h"
#include "Plugin/LoadedPlugin.h"
#include "Plugin/PluginRegistrationScope.h"

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

extern "C" void FALCON_BUILD_GUARD() {
}

namespace {
    bool gLoaded = false;

    const InternalPluginBuild &serverBuild() {
        static const InternalPluginBuild build = FALCON_INTERNAL_BUILD;
        return build;
    }

    const char *textOf(const char *value) {
        return value == nullptr ? "unknown" : value;
    }

    bool sameText(const char *left, const char *right) {
        return left != nullptr && right != nullptr && std::strcmp(left, right) == 0;
    }

    std::string describe(const InternalPluginBuild &build) {
        return std::string(textOf(build.mCommitId)) + " build " + textOf(build.mBuildId);
    }

    void discard(LoadedPlugin &plugin) {
        BlockClassRegistry::remove(&plugin);
        ItemClassRegistry::remove(&plugin);
        ActorClassRegistry::remove(&plugin);
        BlockActorClassRegistry::remove(&plugin);
    }

    void activate(LoadedPlugin &plugin) {
        BlockClassRegistry::activate(&plugin);
        ItemClassRegistry::activate(&plugin);
        ActorClassRegistry::activate(&plugin);
        BlockActorClassRegistry::activate(&plugin);
    }

    bool openInert(LoadedPlugin &plugin, const std::string &path, std::string &error) {
        const PluginRegistrationScope scope(&plugin, false);
        return plugin.mLibrary->open(path, error);
    }

    int enter(InternalPluginEntry entry, LoadedPlugin &plugin, ServerNetworkHandler &owner) {
        const PluginRegistrationScope scope(&plugin, false);
        try {
            return entry(owner, plugin, &plugin.mCallbacks);
        } catch (...) {
            return 0;
        }
    }

#if defined(_WIN32)
    const char *const EXECUTABLE_NAME = "FalconServer.exe";

    std::string runningExecutable() {
        char path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
        return std::filesystem::path(std::string(path, length)).filename().string();
    }
#endif
}

bool InternalPluginLoader::load(LoadedPlugin &plugin, ServerNetworkHandler &owner) {
    const std::string &name = plugin.mDescription.mName;
    const std::string path = (std::filesystem::path(plugin.mDirectory)
                              / NativeLibrary::fileName(plugin.mDescription.mMain)).string();
    const InternalPluginBuild &server = serverBuild();

#if defined(_WIN32)
    const std::string executable = runningExecutable();
    if (lstrcmpiA(executable.c_str(), EXECUTABLE_NAME) != 0) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: internal plugins link against %s, but the server "
                                     "runs as %s", name.c_str(), EXECUTABLE_NAME, executable.c_str());
        return false;
    }
#endif

    plugin.mLibrary = std::make_unique<NativeLibrary>();
    std::string error;
    if (!openInert(plugin, path, error)) {
        discard(plugin);
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s from %s: %s. Internal plugins must be built against "
                                     "this exact server (Falcon %s, %s)", name.c_str(), path.c_str(), error.c_str(),
                  describe(server).c_str(), server.mCompiler);
        return false;
    }

    const auto query = reinterpret_cast<InternalPluginBuildQuery>(
            plugin.mLibrary->symbol(FALCON_INTERNAL_PLUGIN_BUILD_NAME));
    const auto entry = reinterpret_cast<InternalPluginEntry>(
            plugin.mLibrary->symbol(FALCON_INTERNAL_PLUGIN_ENTRY_NAME));
    plugin.mLibrary->detach();

    if (query == nullptr || entry == nullptr) {
        discard(plugin);
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it does not export %s and %s", name.c_str(),
                  FALCON_INTERNAL_PLUGIN_BUILD_NAME, FALCON_INTERNAL_PLUGIN_ENTRY_NAME);
        return false;
    }

    const InternalPluginBuild *build = query();
    if (build == nullptr || !sameText(build->mCommitId, server.mCommitId)
        || !sameText(build->mBuildId, server.mBuildId)) {
        discard(plugin);
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it was built against Falcon %s, this server is "
                                     "Falcon %s", name.c_str(), build == nullptr ? "unknown" : describe(*build).c_str(),
                  describe(server).c_str());
        return false;
    }

    if (!sameText(build->mCompiler, server.mCompiler)) {
        discard(plugin);
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it was compiled with %s, this server was compiled "
                                     "with %s", name.c_str(), textOf(build->mCompiler), server.mCompiler);
        return false;
    }

    if (enter(entry, plugin, owner) == 0) {
        discard(plugin);
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it refused to initialise", name.c_str());
        return false;
    }

    activate(plugin);
    gLoaded = true;
    LOG_INFO(LogAreaID::Server, "Loaded internal plugin %s %s", name.c_str(), plugin.mDescription.mVersion.c_str());
    return true;
}

void InternalPluginLoader::refresh() {
    if (!gLoaded)
        return;

    VanillaBlocks::refreshOverrides();
    VanillaItems::refreshOverrides();
    ActorClassRegistry::resetPrototypes();
}

void InternalPluginLoader::unload(LoadedPlugin &plugin) {
    discard(plugin);
    refresh();
}
