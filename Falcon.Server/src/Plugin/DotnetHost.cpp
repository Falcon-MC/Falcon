#include "Plugin/DotnetHost.h"

#include "Core/Debug/BedrockLog.h"
#include "Plugin/LoadedPlugin.h"
#include "Plugin/NativeLibrary.h"
#include "Plugin/PluginManager.h"
#include "Plugin/PluginServerApi.h"

#include <falcon/falcon_api.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#define DOTNET_HOST_CALLTYPE __cdecl
#define DOTNET_DELEGATE_CALLTYPE __stdcall
#else
#define DOTNET_HOST_CALLTYPE
#define DOTNET_DELEGATE_CALLTYPE
#endif

namespace {
    namespace fs = std::filesystem;

#if defined(_WIN32)
    typedef wchar_t DotnetChar;
#else
    typedef char DotnetChar;
#endif

    typedef std::basic_string<DotnetChar> DotnetString;

    struct HostfxrParameters {
        size_t mSize;
        const DotnetChar *mHostPath;
        const DotnetChar *mDotnetRoot;
    };

    typedef void(DOTNET_HOST_CALLTYPE *HostfxrErrorWriter)(const DotnetChar *message);

    typedef HostfxrErrorWriter(DOTNET_HOST_CALLTYPE *HostfxrSetErrorWriter)(HostfxrErrorWriter writer);

    typedef int32_t(DOTNET_HOST_CALLTYPE *HostfxrInitialize)(const DotnetChar *runtimeConfig,
                                                             const HostfxrParameters *parameters, void **context);

    typedef int32_t(DOTNET_HOST_CALLTYPE *HostfxrGetDelegate)(void *context, int32_t type, void **function);

    typedef int32_t(DOTNET_HOST_CALLTYPE *HostfxrClose)(void *context);

    typedef int(DOTNET_DELEGATE_CALLTYPE *LoadAssemblyAndGetFunctionPointer)(const DotnetChar *assemblyPath,
                                                                              const DotnetChar *typeName,
                                                                              const DotnetChar *methodName,
                                                                              const DotnetChar *delegateTypeName,
                                                                              void *reserved, void **function);

    typedef int (*PluginLoaderEntry)(const FalconServerApi *api, FalconPlugin *plugin,
                                     FalconPluginCallbacks *callbacks, const char *assemblyPath,
                                     const char *mainClass, const char *const *dependencies,
                                     uint32_t dependencyCount);

#if defined(_WIN32)
    const char *const HOSTFXR_LIBRARY = "hostfxr.dll";
#elif defined(__APPLE__)
    const char *const HOSTFXR_LIBRARY = "libhostfxr.dylib";
#else
    const char *const HOSTFXR_LIBRARY = "libhostfxr.so";
#endif

    const char *const API_ASSEMBLY = "Falcon.PluginAPI.dll";
    const char *const API_RUNTIME_CONFIG = "Falcon.PluginAPI.runtimeconfig.json";
    const char *const SHARED_DIRECTORY = ".dotnet";
    const char *const LOADER_TYPE = "Falcon.Hosting.PluginLoader, Falcon.PluginAPI";
    const char *const LOADER_METHOD = "Load";
    const int32_t LOAD_ASSEMBLY_AND_GET_FUNCTION_POINTER = 5;
    const DotnetChar *const UNMANAGED_CALLERS_ONLY = reinterpret_cast<const DotnetChar *>(static_cast<intptr_t>(-1));

    HostfxrSetErrorWriter gSetErrorWriter = nullptr;
    LoadAssemblyAndGetFunctionPointer gLoadAssembly = nullptr;
    PluginLoaderEntry gEntry = nullptr;
    std::string gRuntimeFailure;
    std::string gHostErrors;

    std::string environment(const char *name) {
        const char *value = std::getenv(name);
        return value == nullptr ? std::string() : std::string(value);
    }

    DotnetString toDotnet(const std::string &utf8) {
        return fs::u8path(utf8).native();
    }

    std::string fromDotnet(const DotnetChar *value) {
        if (value == nullptr)
            return std::string();
        return fs::path(DotnetString(value)).u8string();
    }

    void DOTNET_HOST_CALLTYPE collectHostError(const DotnetChar *message) {
        if (!gHostErrors.empty())
            gHostErrors += " ";
        gHostErrors += fromDotnet(message);
    }

    class HostErrorCapture {
    public:
        HostErrorCapture() {
            gHostErrors.clear();
            if (gSetErrorWriter != nullptr)
                gSetErrorWriter(&collectHostError);
        }

        ~HostErrorCapture() {
            if (gSetErrorWriter != nullptr)
                gSetErrorWriter(nullptr);
        }

        HostErrorCapture(const HostErrorCapture &) = delete;

        HostErrorCapture &operator=(const HostErrorCapture &) = delete;
    };

    std::string describeFailure(const std::string &step, int32_t code) {
        char hex[16] = {};
        std::snprintf(hex, sizeof(hex), "0x%08x", (uint32_t) code);
        std::string text = step + " failed with " + hex;
        if (!gHostErrors.empty())
            text += ": " + gHostErrors;
        return text;
    }

    std::vector<uint32_t> versionNumbers(const std::string &name, bool &prerelease) {
        const size_t dash = name.find('-');
        prerelease = dash != std::string::npos;

        std::vector<uint32_t> numbers;
        std::string current;
        for (const char character: name.substr(0, dash) + ".") {
            if (character != '.') {
                if (character < '0' || character > '9' || current.size() >= 9)
                    return std::vector<uint32_t>();
                current += character;
                continue;
            }

            if (current.empty())
                return std::vector<uint32_t>();
            numbers.push_back((uint32_t) std::stoul(current));
            current.clear();
        }
        return numbers;
    }

    bool isNewer(const std::string &candidate, const std::string &current) {
        bool candidatePrerelease = false;
        bool currentPrerelease = false;
        const std::vector<uint32_t> left = versionNumbers(candidate, candidatePrerelease);
        const std::vector<uint32_t> right = versionNumbers(current, currentPrerelease);
        if (left != right)
            return left > right;
        return currentPrerelease && !candidatePrerelease;
    }

    bool findHostfxrIn(const fs::path &root, fs::path &library) {
        const fs::path versions = root / "host" / "fxr";
        std::error_code errorCode;
        std::string best;
        for (const fs::directory_entry &entry: fs::directory_iterator(versions, errorCode)) {
            const std::string version = entry.path().filename().string();
            bool prerelease = false;
            if (versionNumbers(version, prerelease).empty())
                continue;
            if (!fs::is_regular_file(entry.path() / HOSTFXR_LIBRARY, errorCode))
                continue;
            if (best.empty() || isNewer(version, best))
                best = version;
        }

        if (best.empty())
            return false;
        library = versions / best / HOSTFXR_LIBRARY;
        return true;
    }

    std::vector<fs::path> dotnetRoots() {
        std::vector<fs::path> roots;
        const std::string configured = environment("DOTNET_ROOT");
        if (!configured.empty())
            roots.emplace_back(configured);

        roots.emplace_back("C:/Program Files/dotnet");
        roots.emplace_back("/usr/share/dotnet");
        roots.emplace_back("/usr/lib/dotnet");
        roots.emplace_back("/usr/local/share/dotnet");
        roots.emplace_back("/opt/homebrew/opt/dotnet/libexec");

#if defined(_WIN32)
        const std::string home = environment("USERPROFILE");
#else
        const std::string home = environment("HOME");
#endif
        if (!home.empty())
            roots.push_back(fs::path(home) / ".dotnet");
        return roots;
    }

    bool locateHostfxr(fs::path &library, fs::path &root, std::string &error) {
        std::string searched;
        for (const fs::path &candidate: dotnetRoots()) {
            if (findHostfxrIn(candidate, library)) {
                std::error_code errorCode;
                root = fs::absolute(candidate, errorCode);
                if (errorCode)
                    root = candidate;
                return true;
            }

            if (!searched.empty())
                searched += ", ";
            searched += candidate.string();
        }

        error = std::string("no ") + HOSTFXR_LIBRARY + " was found under host/fxr in " + searched
                + ". Install the .NET 8 runtime or set DOTNET_ROOT";
        return false;
    }

    bool findApiDirectory(const fs::path &pluginDirectory, fs::path &directory) {
        const fs::path candidates[] = {pluginDirectory.parent_path() / SHARED_DIRECTORY, pluginDirectory};
        for (const fs::path &candidate: candidates) {
            std::error_code errorCode;
            if (!fs::is_regular_file(candidate / API_ASSEMBLY, errorCode)
                || !fs::is_regular_file(candidate / API_RUNTIME_CONFIG, errorCode))
                continue;

            directory = fs::absolute(candidate, errorCode);
            if (errorCode)
                directory = candidate;
            return true;
        }
        return false;
    }

    bool startRuntime(const fs::path &apiDirectory, std::string &error) {
        fs::path library;
        fs::path root;
        if (!locateHostfxr(library, root, error))
            return false;

        NativeLibrary hostfxr;
        if (!hostfxr.open(library.string(), error)) {
            error = "could not open " + library.string() + ": " + error;
            return false;
        }

        const auto initialize = reinterpret_cast<HostfxrInitialize>(
                hostfxr.symbol("hostfxr_initialize_for_runtime_config"));
        const auto getDelegate = reinterpret_cast<HostfxrGetDelegate>(hostfxr.symbol("hostfxr_get_runtime_delegate"));
        const auto close = reinterpret_cast<HostfxrClose>(hostfxr.symbol("hostfxr_close"));
        if (initialize == nullptr || getDelegate == nullptr || close == nullptr) {
            error = library.string() + " is too old to host plugins, install the .NET 8 runtime";
            return false;
        }

        gSetErrorWriter = reinterpret_cast<HostfxrSetErrorWriter>(hostfxr.symbol("hostfxr_set_error_writer"));
        hostfxr.detach();

        const HostErrorCapture capture;
        const fs::path runtimeConfig = apiDirectory / API_RUNTIME_CONFIG;
        const DotnetString dotnetRoot = root.native();
        const HostfxrParameters parameters{sizeof(HostfxrParameters), nullptr, dotnetRoot.c_str()};

        void *context = nullptr;
        const int32_t initialized = initialize(runtimeConfig.native().c_str(), &parameters, &context);
        if (initialized < 0 || context == nullptr) {
            error = describeFailure("Starting the runtime from " + runtimeConfig.u8string() + " with "
                                    + library.u8string(), initialized);
            return false;
        }

        void *loadAssembly = nullptr;
        const int32_t obtained = getDelegate(context, LOAD_ASSEMBLY_AND_GET_FUNCTION_POINTER, &loadAssembly);
        close(context);
        if (obtained < 0 || loadAssembly == nullptr) {
            error = describeFailure("Getting the assembly loader of the runtime", obtained);
            return false;
        }

        gLoadAssembly = reinterpret_cast<LoadAssemblyAndGetFunctionPointer>(loadAssembly);
        return true;
    }

    bool resolveLoader(const fs::path &apiDirectory, std::string &error) {
        const HostErrorCapture capture;
        const fs::path assembly = apiDirectory / API_ASSEMBLY;
        const DotnetString type = toDotnet(LOADER_TYPE);
        const DotnetString method = toDotnet(LOADER_METHOD);

        void *entry = nullptr;
        const int resolved = gLoadAssembly(assembly.native().c_str(), type.c_str(), method.c_str(),
                                           UNMANAGED_CALLERS_ONLY, nullptr, &entry);
        if (resolved < 0 || entry == nullptr) {
            error = describeFailure("Loading " + std::string(LOADER_TYPE) + " from " + assembly.u8string(), resolved);
            return false;
        }

        gEntry = reinterpret_cast<PluginLoaderEntry>(entry);
        return true;
    }

    bool prepare(const fs::path &pluginDirectory, std::string &error) {
        if (gEntry != nullptr)
            return true;

        if (!gRuntimeFailure.empty()) {
            error = "the .NET runtime could not be started: " + gRuntimeFailure;
            return false;
        }

        fs::path apiDirectory;
        if (!findApiDirectory(pluginDirectory, apiDirectory)) {
            error = std::string(API_ASSEMBLY) + " and " + API_RUNTIME_CONFIG + " were found neither in "
                    + (pluginDirectory.parent_path() / SHARED_DIRECTORY).string() + " nor in "
                    + pluginDirectory.string();
            return false;
        }

        if (gLoadAssembly == nullptr && !startRuntime(apiDirectory, gRuntimeFailure)) {
            error = "the .NET runtime could not be started: " + gRuntimeFailure;
            return false;
        }

        return resolveLoader(apiDirectory, error);
    }
}

bool DotnetHost::load(LoadedPlugin &plugin) {
    const std::string &name = plugin.mDescription.mName;
    const fs::path directory(plugin.mDirectory);
    const fs::path relative = directory / plugin.mDescription.mAssembly;

    std::error_code errorCode;
    const fs::path assembly = fs::absolute(relative, errorCode);
    if (errorCode || !fs::is_regular_file(assembly, errorCode)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s does not exist", name.c_str(),
                  relative.string().c_str());
        return false;
    }

    std::string error;
    if (!prepare(directory, error)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s", name.c_str(), error.c_str());
        return false;
    }

    std::vector<const char *> dependencies;
    for (const std::string &dependency: plugin.mDescription.mDepend)
        dependencies.push_back(dependency.c_str());
    for (const std::string &dependency: plugin.mDescription.mSoftDepend)
        dependencies.push_back(dependency.c_str());

    const std::string assemblyPath = assembly.u8string();
    int accepted = 0;
    try {
        accepted = gEntry(&PluginServerApi::get(), PluginManager::toHandle(plugin), &plugin.mCallbacks,
                          assemblyPath.c_str(), plugin.mDescription.mMain.c_str(), dependencies.data(),
                          (uint32_t) dependencies.size());
    } catch (...) {
        accepted = 0;
    }

    if (accepted == 0) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it refused to initialise", name.c_str());
        return false;
    }

    LOG_INFO(LogAreaID::Server, "Loaded .NET plugin %s %s", name.c_str(), plugin.mDescription.mVersion.c_str());
    return true;
}
