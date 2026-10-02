#include "Plugin/JvmHost.h"

#include "Core/Debug/BedrockLog.h"
#include "Plugin/JvmNative.h"
#include "Plugin/LoadedPlugin.h"
#include "Plugin/NativeLibrary.h"
#include "Plugin/PluginManager.h"
#include "Plugin/PluginServerApi.h"

#include <falcon/falcon_api.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
    namespace fs = std::filesystem;

    constexpr int MINIMUM_JAVA_FEATURE = 22;
    constexpr JvmNative::Int LOCAL_FRAME_CAPACITY = 16;
    const char *const SDK_DIRECTORY = ".java";
    const char *const SDK_PREFIX = "falcon-plugin-api-";
    const char *const SDK_SUFFIX = ".jar";
    const char *const LOADER_CLASS = "falcon/api/internal/Loader";
    const char *const LOADER_METHOD = "load";
    const char *const LOADER_SIGNATURE = "(JJJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z";
    const char *const OPTIONS_VARIABLE = "FALCON_JAVA_OPTIONS";

    enum class HostState {
        Idle,
        Running,
        Failed
    };

    struct SdkVersion {
        uint32_t mMajor = 0;
        uint32_t mMinor = 0;
        uint32_t mPatch = 0;
    };

    struct AsyncCall {
        FalconTask mWork;
        FalconTask mDone;
        void *mUserData;
    };

    HostState gState = HostState::Idle;
    std::string gFailure;
    JvmNative::Vm *gVm = nullptr;
    FalconServerApi gApi{};

    template<typename Function>
    Function tableEntry(void *const *table, size_t index) {
        return reinterpret_cast<Function>(table[index]);
    }

    class Environment {
    public:
        explicit Environment(JvmNative::Env *env) : mEnv(env) {
        }

        bool pushFrame() const {
            const auto push = entry<JvmNative::PushLocalFrameFunction>(JvmNative::EnvIndex::PUSH_LOCAL_FRAME);
            return push(mEnv, LOCAL_FRAME_CAPACITY) == JvmNative::RESULT_OK;
        }

        void popFrame() const {
            entry<JvmNative::PopLocalFrameFunction>(JvmNative::EnvIndex::POP_LOCAL_FRAME)(mEnv, nullptr);
        }

        JvmNative::Object findClass(const char *name) const {
            return entry<JvmNative::FindClassFunction>(JvmNative::EnvIndex::FIND_CLASS)(mEnv, name);
        }

        JvmNative::Object classOf(JvmNative::Object object) const {
            return entry<JvmNative::GetObjectClassFunction>(JvmNative::EnvIndex::GET_OBJECT_CLASS)(mEnv, object);
        }

        JvmNative::MethodId method(JvmNative::Object type, const char *name, const char *signature) const {
            const auto find = entry<JvmNative::GetMethodIdFunction>(JvmNative::EnvIndex::GET_METHOD_ID);
            return find(mEnv, type, name, signature);
        }

        JvmNative::MethodId staticMethod(JvmNative::Object type, const char *name, const char *signature) const {
            const auto find = entry<JvmNative::GetMethodIdFunction>(JvmNative::EnvIndex::GET_STATIC_METHOD_ID);
            return find(mEnv, type, name, signature);
        }

        JvmNative::Object callObject(JvmNative::Object target, JvmNative::MethodId id) const {
            const auto call = entry<JvmNative::CallObjectMethodFunction>(JvmNative::EnvIndex::CALL_OBJECT_METHOD_A);
            return call(mEnv, target, id, nullptr);
        }

        JvmNative::Int callInt(JvmNative::Object target, JvmNative::MethodId id) const {
            const auto call = entry<JvmNative::CallIntMethodFunction>(JvmNative::EnvIndex::CALL_INT_METHOD_A);
            return call(mEnv, target, id, nullptr);
        }

        JvmNative::Object callStaticObject(JvmNative::Object type, JvmNative::MethodId id) const {
            const size_t index = JvmNative::EnvIndex::CALL_STATIC_OBJECT_METHOD_A;
            return entry<JvmNative::CallObjectMethodFunction>(index)(mEnv, type, id, nullptr);
        }

        bool callStaticBoolean(JvmNative::Object type, JvmNative::MethodId id, const JvmNative::Value *values) const {
            const size_t index = JvmNative::EnvIndex::CALL_STATIC_BOOLEAN_METHOD_A;
            return entry<JvmNative::CallBooleanMethodFunction>(index)(mEnv, type, id, values) != 0;
        }

        JvmNative::Object newString(const std::string &text) const {
            return entry<JvmNative::NewStringUtfFunction>(JvmNative::EnvIndex::NEW_STRING_UTF)(mEnv, text.c_str());
        }

        bool failed() const {
            return entry<JvmNative::ExceptionCheckFunction>(JvmNative::EnvIndex::EXCEPTION_CHECK)(mEnv) != 0;
        }

        std::string takeException() const {
            const auto occurred = entry<JvmNative::ExceptionOccurredFunction>(JvmNative::EnvIndex::EXCEPTION_OCCURRED);
            const JvmNative::Object throwable = occurred(mEnv);
            if (throwable == nullptr)
                return "unknown error";

            _clearException();
            const JvmNative::MethodId toString = method(classOf(throwable), "toString", "()Ljava/lang/String;");
            if (toString == nullptr) {
                _clearException();
                return "unknown error";
            }

            const JvmNative::Object text = callObject(throwable, toString);
            if (text == nullptr || failed()) {
                _clearException();
                return "unknown error";
            }
            return _stringOf(text);
        }

    private:
        template<typename Function>
        Function entry(size_t index) const {
            return tableEntry<Function>(mEnv->mFunctions, index);
        }

        void _clearException() const {
            entry<JvmNative::ExceptionClearFunction>(JvmNative::EnvIndex::EXCEPTION_CLEAR)(mEnv);
        }

        std::string _stringOf(JvmNative::Object text) const {
            const auto acquire = entry<JvmNative::GetStringUtfCharsFunction>(JvmNative::EnvIndex::GET_STRING_UTF_CHARS);
            const char *characters = acquire(mEnv, text, nullptr);
            if (characters == nullptr) {
                _clearException();
                return "unknown error";
            }

            std::string result(characters);
            const size_t index = JvmNative::EnvIndex::RELEASE_STRING_UTF_CHARS;
            entry<JvmNative::ReleaseStringUtfCharsFunction>(index)(mEnv, text, characters);
            return result;
        }

        JvmNative::Env *mEnv;
    };

    class ThreadAttachment {
    public:
        ThreadAttachment() {
            if (gVm == nullptr)
                return;

            void *env = nullptr;
            const auto getEnv = tableEntry<JvmNative::GetEnvFunction>(gVm->mFunctions, JvmNative::VmIndex::GET_ENV);
            const JvmNative::Int result = getEnv(gVm, &env, JvmNative::VERSION_21);
            if (result == JvmNative::RESULT_OK) {
                mEnv = static_cast<JvmNative::Env *>(env);
                return;
            }
            if (result != JvmNative::RESULT_DETACHED)
                return;

            const size_t index = JvmNative::VmIndex::ATTACH_CURRENT_THREAD_AS_DAEMON;
            const auto attach = tableEntry<JvmNative::AttachCurrentThreadFunction>(gVm->mFunctions, index);
            if (attach(gVm, &env, nullptr) != JvmNative::RESULT_OK)
                return;

            mEnv = static_cast<JvmNative::Env *>(env);
            mAttached = true;
        }

        ~ThreadAttachment() {
            if (!mAttached)
                return;

            const size_t index = JvmNative::VmIndex::DETACH_CURRENT_THREAD;
            tableEntry<JvmNative::DetachCurrentThreadFunction>(gVm->mFunctions, index)(gVm);
        }

        ThreadAttachment(const ThreadAttachment &) = delete;

        ThreadAttachment &operator=(const ThreadAttachment &) = delete;

        bool ready() const {
            return mEnv != nullptr;
        }

        Environment environment() const {
            return Environment(mEnv);
        }

    private:
        JvmNative::Env *mEnv = nullptr;
        bool mAttached = false;
    };

    void runAsyncWork(void *userData) {
        const AsyncCall *call = static_cast<const AsyncCall *>(userData);
        const ThreadAttachment attachment;
        if (!attachment.ready()) {
            LOG_ERROR(LogAreaID::Server, "Could not attach the plugin worker thread to the Java runtime");
            return;
        }
        call->mWork(call->mUserData);
    }

    void runAsyncDone(void *userData) {
        const std::unique_ptr<AsyncCall> call(static_cast<AsyncCall *>(userData));
        if (call->mDone != nullptr)
            call->mDone(call->mUserData);
    }

    uint64_t runAsyncAttached(FalconPlugin *plugin, FalconTask work, FalconTask done, void *userData) {
        if (work == nullptr)
            return 0;

        auto *call = new AsyncCall{work, done, userData};
        const uint64_t id = PluginServerApi::get().runAsync(plugin, &runAsyncWork, &runAsyncDone, call);
        if (id == 0)
            delete call;
        return id;
    }

    bool parseNumber(const std::string &text, uint32_t &value) {
        if (text.empty() || text.size() > 9)
            return false;

        for (const char character: text) {
            if (character < '0' || character > '9')
                return false;
        }
        value = (uint32_t) std::stoul(text);
        return true;
    }

    int featureOf(const std::string &version) {
        const size_t firstDot = version.find_first_of(".-+_");
        uint32_t first = 0;
        if (!parseNumber(version.substr(0, firstDot), first))
            return 0;
        if (first != 1 || firstDot == std::string::npos)
            return (int) first;

        const size_t secondDot = version.find_first_of(".-+_", firstDot + 1);
        uint32_t second = 0;
        if (!parseNumber(version.substr(firstDot + 1, secondDot - firstDot - 1), second))
            return 0;
        return (int) second;
    }

    int releaseFeature(const fs::path &home) {
        std::ifstream file(home / "release");
        std::string line;
        const std::string key = "JAVA_VERSION=";
        while (std::getline(file, line)) {
            if (line.rfind(key, 0) != 0)
                continue;

            std::string value = line.substr(key.size());
            if (value.size() >= 2 && value.front() == '"')
                value = value.substr(1, value.find('"', 1) - 1);
            return featureOf(value);
        }
        return 0;
    }

    fs::path javaHome() {
        for (const char *variable: {"FALCON_JAVA_HOME", "JAVA_HOME"}) {
            const char *value = std::getenv(variable);
            if (value != nullptr && *value != '\0')
                return fs::path(value);
        }
        return fs::path();
    }

    fs::path findLibrary(const fs::path &home) {
#if defined(_WIN32)
        const std::vector<fs::path> candidates = {home / "bin" / "server" / "jvm.dll"};
#elif defined(__APPLE__)
        const std::vector<fs::path> candidates = {home / "lib" / "server" / "libjvm.dylib",
                                                  home / "Contents" / "Home" / "lib" / "server" / "libjvm.dylib"};
#else
        const std::vector<fs::path> candidates = {home / "lib" / "server" / "libjvm.so"};
#endif
        std::error_code errorCode;
        for (const fs::path &candidate: candidates) {
            if (fs::is_regular_file(candidate, errorCode))
                return candidate;
        }
        return fs::path();
    }

    bool parseSdkVersion(const std::string &text, SdkVersion &version) {
        const std::string plain = text.substr(0, text.find('-'));
        const size_t firstDot = plain.find('.');
        if (firstDot == std::string::npos)
            return false;

        const size_t secondDot = plain.find('.', firstDot + 1);
        const std::string minor = plain.substr(firstDot + 1, secondDot - firstDot - 1);
        const std::string patch = secondDot == std::string::npos ? "0" : plain.substr(secondDot + 1);
        return parseNumber(plain.substr(0, firstDot), version.mMajor) && parseNumber(minor, version.mMinor)
               && parseNumber(patch, version.mPatch);
    }

    bool newerThan(const SdkVersion &left, const SdkVersion &right) {
        if (left.mMinor != right.mMinor)
            return left.mMinor > right.mMinor;
        return left.mPatch > right.mPatch;
    }

    fs::path findSdk(const fs::path &directory) {
        const std::string prefix = SDK_PREFIX;
        const std::string suffix = SDK_SUFFIX;
        fs::path best;
        SdkVersion bestVersion;

        std::error_code errorCode;
        for (const fs::directory_entry &entry: fs::directory_iterator(directory, errorCode)) {
            const std::string name = entry.path().filename().string();
            if (!entry.is_regular_file(errorCode) || name.size() <= prefix.size() + suffix.size()
                || name.rfind(prefix, 0) != 0 || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
                continue;

            SdkVersion version;
            const std::string text = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
            if (!parseSdkVersion(text, version) || version.mMajor != FALCON_API_VERSION_MAJOR
                || version.mMinor > FALCON_API_VERSION_MINOR)
                continue;

            if (best.empty() || newerThan(version, bestVersion)) {
                best = entry.path();
                bestVersion = version;
            }
        }
        return best;
    }

    std::vector<std::string> runtimeOptions(const fs::path &sdk) {
        std::vector<std::string> options;
        options.push_back("-Djava.class.path=" + sdk.string());
        options.push_back("--enable-native-access=ALL-UNNAMED");
        options.push_back("-Xrs");

        const char *extra = std::getenv(OPTIONS_VARIABLE);
        if (extra == nullptr)
            return options;

        std::istringstream stream(extra);
        std::string option;
        while (stream >> option)
            options.push_back(option);
        return options;
    }

    bool openLibrary(NativeLibrary &library, const fs::path &home, const fs::path &path, std::string &error) {
#if defined(_WIN32)
        SetDllDirectoryW((home / "bin").wstring().c_str());
        const bool opened = library.open(path.string(), error);
        SetDllDirectoryW(nullptr);
        return opened;
#else
        (void) home;
        return library.open(path.string(), error);
#endif
    }

    int runtimeFeature(const Environment &environment) {
        const JvmNative::Object runtime = environment.findClass("java/lang/Runtime");
        if (runtime == nullptr)
            return 0;

        const JvmNative::MethodId versionMethod = environment.staticMethod(runtime, "version",
                                                                           "()Ljava/lang/Runtime$Version;");
        if (versionMethod == nullptr)
            return 0;

        const JvmNative::Object version = environment.callStaticObject(runtime, versionMethod);
        if (version == nullptr || environment.failed())
            return 0;

        const JvmNative::MethodId featureMethod = environment.method(environment.classOf(version), "feature", "()I");
        if (featureMethod == nullptr)
            return 0;

        const JvmNative::Int feature = environment.callInt(version, featureMethod);
        return environment.failed() ? 0 : (int) feature;
    }

    bool checkRuntime(std::string &error) {
        const ThreadAttachment attachment;
        if (!attachment.ready()) {
            error = "the Java runtime started but this thread cannot use it";
            return false;
        }

        const Environment environment = attachment.environment();
        if (!environment.pushFrame()) {
            error = "the Java runtime is out of memory";
            return false;
        }

        const int feature = runtimeFeature(environment);
        if (feature == 0)
            error = "could not read the Java version: " + environment.takeException();
        environment.popFrame();

        if (feature == 0)
            return false;
        if (feature < MINIMUM_JAVA_FEATURE) {
            error = "Java plugins need Java " + std::to_string(MINIMUM_JAVA_FEATURE) + " or newer, the runtime is Java "
                    + std::to_string(feature);
            return false;
        }
        return true;
    }

    bool startRuntime(const fs::path &pluginDirectory, std::string &error) {
        const fs::path home = javaHome();
        if (home.empty()) {
            error = "Java plugins need Java " + std::to_string(MINIMUM_JAVA_FEATURE)
                    + " or newer: set FALCON_JAVA_HOME or JAVA_HOME to its installation";
            return false;
        }

        const int feature = releaseFeature(home);
        if (feature != 0 && feature < MINIMUM_JAVA_FEATURE) {
            error = "Java plugins need Java " + std::to_string(MINIMUM_JAVA_FEATURE) + " or newer, " + home.string()
                    + " is Java " + std::to_string(feature);
            return false;
        }

        const fs::path library = findLibrary(home);
        if (library.empty()) {
            error = "no Java virtual machine library found in " + home.string();
            return false;
        }

        const fs::path sdkDirectory = pluginDirectory / SDK_DIRECTORY;
        const fs::path sdk = findSdk(sdkDirectory);
        if (sdk.empty()) {
            error = "no " + std::string(SDK_PREFIX) + std::to_string(FALCON_API_VERSION_MAJOR) + ".x.jar for API "
                    + std::to_string(FALCON_API_VERSION_MAJOR) + "." + std::to_string(FALCON_API_VERSION_MINOR)
                    + " in " + sdkDirectory.string();
            return false;
        }

        NativeLibrary runtime;
        if (!openLibrary(runtime, home, library, error)) {
            error = "could not open " + library.string() + ": " + error;
            return false;
        }

        const auto create = reinterpret_cast<JvmNative::CreateJavaVmFunction>(runtime.symbol("JNI_CreateJavaVM"));
        if (create == nullptr) {
            error = library.string() + " does not export JNI_CreateJavaVM";
            return false;
        }
        runtime.detach();

        std::error_code errorCode;
        const fs::path sdkPath = fs::absolute(sdk, errorCode);
        std::vector<std::string> options = runtimeOptions(errorCode ? sdk : sdkPath);
        std::vector<JvmNative::Option> values;
        values.reserve(options.size());
        for (std::string &option: options)
            values.push_back(JvmNative::Option{&option[0], nullptr});

        JvmNative::InitArgs arguments{};
        arguments.mVersion = JvmNative::VERSION_21;
        arguments.mOptionCount = (JvmNative::Int) values.size();
        arguments.mOptions = values.data();
        arguments.mIgnoreUnrecognized = 0;

        void *env = nullptr;
        const JvmNative::Int result = create(&gVm, &env, &arguments);
        if (result != JvmNative::RESULT_OK || gVm == nullptr) {
            gVm = nullptr;
            error = "the Java virtual machine in " + home.string() + " failed to start (error "
                    + std::to_string(result) + ")";
            return false;
        }

        if (!checkRuntime(error))
            return false;

        gApi = PluginServerApi::get();
        gApi.runAsync = &runAsyncAttached;
        LOG_INFO(LogAreaID::Server, "Started the Java runtime from %s with %s", home.string().c_str(),
                 sdk.filename().string().c_str());
        return true;
    }

    std::string dependenciesOf(const PluginDescription &description) {
        std::string names;
        for (const std::vector<std::string> *list: {&description.mDepend, &description.mSoftDepend}) {
            for (const std::string &name: *list) {
                if (!names.empty())
                    names += '\n';
                names += name;
            }
        }
        return names;
    }

    bool invokeLoader(const Environment &environment, LoadedPlugin &plugin, const std::string &jar,
                      std::string &error) {
        const JvmNative::Object loader = environment.findClass(LOADER_CLASS);
        if (loader == nullptr) {
            error = "the plugin API jar has no loader: " + environment.takeException();
            return false;
        }

        const JvmNative::MethodId method = environment.staticMethod(loader, LOADER_METHOD, LOADER_SIGNATURE);
        if (method == nullptr) {
            error = "the plugin API jar has an incompatible loader: " + environment.takeException();
            return false;
        }

        const std::string texts[] = {jar, plugin.mDescription.mMain, dependenciesOf(plugin.mDescription)};
        JvmNative::Value values[6];
        values[0].j = (JvmNative::Long) (intptr_t) &gApi;
        values[1].j = (JvmNative::Long) (intptr_t) PluginManager::toHandle(plugin);
        values[2].j = (JvmNative::Long) (intptr_t) &plugin.mCallbacks;
        for (size_t index = 0; index < 3; index++) {
            values[3 + index].l = environment.newString(texts[index]);
            if (values[3 + index].l == nullptr) {
                error = environment.takeException();
                return false;
            }
        }

        const bool accepted = environment.callStaticBoolean(loader, method, values);
        if (environment.failed()) {
            error = environment.takeException();
            return false;
        }
        if (!accepted) {
            error = "it refused to initialise";
            return false;
        }
        return true;
    }

    bool enterLoader(LoadedPlugin &plugin, const std::string &jar, std::string &error) {
        const ThreadAttachment attachment;
        if (!attachment.ready()) {
            error = "this thread cannot use the Java runtime";
            return false;
        }

        const Environment environment = attachment.environment();
        if (!environment.pushFrame()) {
            error = "the Java runtime is out of memory";
            return false;
        }

        const bool entered = invokeLoader(environment, plugin, jar, error);
        environment.popFrame();
        return entered;
    }
}

bool JvmHost::load(LoadedPlugin &plugin) {
    const PluginDescription &description = plugin.mDescription;
    const char *name = description.mName.c_str();

    if (description.mApiMajor != FALCON_API_VERSION_MAJOR || description.mApiMinor > FALCON_API_VERSION_MINOR) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it needs API %u.%u, the server provides %u.%u", name,
                  description.mApiMajor, description.mApiMinor, FALCON_API_VERSION_MAJOR, FALCON_API_VERSION_MINOR);
        return false;
    }

    std::error_code errorCode;
    const fs::path jar = fs::absolute(fs::path(plugin.mDirectory) / description.mJar, errorCode);
    if (errorCode || !fs::is_regular_file(jar, errorCode)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s does not exist", name,
                  (fs::path(plugin.mDirectory) / description.mJar).string().c_str());
        return false;
    }

    if (gState == HostState::Idle) {
        const bool started = startRuntime(fs::path(plugin.mDirectory).parent_path(), gFailure);
        gState = started ? HostState::Running : HostState::Failed;
    }

    if (gState == HostState::Failed) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s", name, gFailure.c_str());
        return false;
    }

    std::string error;
    if (!enterLoader(plugin, jar.string(), error)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s", name, error.c_str());
        return false;
    }

    LOG_INFO(LogAreaID::Server, "Loaded Java plugin %s %s", name, description.mVersion.c_str());
    return true;
}
