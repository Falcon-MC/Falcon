#pragma once

#include "BuildInfo.h"

#include <falcon/falcon_api.h>

#include <cstdint>

class ServerNetworkHandler;
struct LoadedPlugin;

#if defined(__VERSION__)
#define FALCON_INTERNAL_COMPILER_VERSION __VERSION__
#else
#define FALCON_INTERNAL_COMPILER_VERSION "unknown"
#endif

#if defined(_WIN32) && defined(_UCRT)
#define FALCON_INTERNAL_RUNTIME " ucrt"
#elif defined(_WIN32)
#define FALCON_INTERNAL_RUNTIME " msvcrt"
#else
#define FALCON_INTERNAL_RUNTIME ""
#endif

#define FALCON_INTERNAL_COMPILER FALCON_INTERNAL_COMPILER_VERSION FALCON_INTERNAL_RUNTIME

#define FALCON_INTERNAL_PLUGIN_BUILD_NAME "falcon_internal_plugin_build"
#define FALCON_INTERNAL_PLUGIN_ENTRY_NAME "falcon_internal_plugin_entry"

#define FALCON_INTERNAL_BUILD \
    {FALCON_BUILD_COMMIT_ID, FALCON_BUILD_NUMBER, FALCON_INTERNAL_COMPILER, &FALCON_BUILD_GUARD}

extern "C" void FALCON_BUILD_GUARD();

struct InternalPluginBuild {
    const char *mCommitId;
    const char *mBuildId;
    const char *mCompiler;
    void (*mGuard)();
};

using InternalPluginBuildQuery = const InternalPluginBuild *(*)();

using InternalPluginEntry = int (*)(ServerNetworkHandler &server, LoadedPlugin &plugin,
                                    FalconPluginCallbacks *callbacks);
