#include "Falcon/Plugin/WasmHost.h"

#include "Core/Debug/BedrockLog.h"
#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/WasmPlugin.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"

#include <wasmtime.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
    constexpr uint64_t HOST_CALL_FUEL = 1000;
    constexpr int64_t MAX_TABLE_ELEMENTS = 100000;
    constexpr int64_t MAX_TABLES = 10;

    namespace fs = std::filesystem;

    const char *const PACKAGE_PREFIX = "falcon:plugin/";
    const char *const PACKAGE_SUFFIX = "@1.0.5";

    wasm_valkind_t kindOf(char letter) {
        switch (letter) {
            case 'I':
                return WASM_I64;
            case 'f':
                return WASM_F32;
            case 'F':
                return WASM_F64;
            default:
                return WASM_I32;
        }
    }

    void fillTypes(wasm_valtype_vec_t &out, const std::string &letters) {
        if (letters.empty()) {
            wasm_valtype_vec_new_empty(&out);
            return;
        }

        std::vector<wasm_valtype_t *> types;
        for (const char letter: letters)
            types.push_back(wasm_valtype_new(kindOf(letter)));
        wasm_valtype_vec_new(&out, types.size(), types.data());
    }

    wasm_functype_t *functionType(const char *signature) {
        const std::string text(signature);
        const size_t separator = text.find(':');
        wasm_valtype_vec_t params;
        wasm_valtype_vec_t results;
        fillTypes(params, text.substr(0, separator));
        fillTypes(results, text.substr(separator + 1));
        return wasm_functype_new(&params, &results);
    }

    std::string errorText(wasmtime_error_t *error) {
        wasm_byte_vec_t message;
        wasmtime_error_message(error, &message);
        std::string text(message.data, message.size);
        wasm_byte_vec_delete(&message);
        wasmtime_error_delete(error);
        return text;
    }

    wasm_trap_t *invoke(void *environment, wasmtime_caller_t *caller, wasmtime_val_raw_t *values, size_t) {
        const auto &definition = *static_cast<const WasmImports::Definition *>(environment);
        auto &plugin = *static_cast<WasmPlugin *>(wasmtime_context_get_data(wasmtime_caller_context(caller)));
        if (plugin.isFaulted()) {
            static const char message[] = "the plugin was stopped";
            return wasmtime_trap_new(message, sizeof(message) - 1);
        }

        // host calls cost fuel too: otherwise one guest call could stall the tick with millions of them
        wasmtime_context_t *context = wasmtime_caller_context(caller);
        uint64_t fuel = 0;
        wasmtime_context_get_fuel(context, &fuel);
        if (fuel < HOST_CALL_FUEL) {
            static const char message[] = "the plugin ran out of fuel in a server call";
            return wasmtime_trap_new(message, sizeof(message) - 1);
        }
        wasmtime_context_set_fuel(context, fuel - HOST_CALL_FUEL);

        WasmCall call(plugin, caller, values);
        // a c++ exception must never unwind through wasm frames
        try {
            definition.mHandler(call);
        } catch (...) {
            call.fail("the server threw an exception");
        }
        return call.takeTrap();
    }

    void dropLevel(WasmCall &call) {
        call.plugin().dropLevel(call.u32(0));
    }

    void dropItem(WasmCall &call) {
        call.plugin().dropItem(call.u32(0));
    }

    const WasmImports::Definition RESOURCE_DROPS[] = {
            {"types", "[resource-drop]level", "i:", &dropLevel},
            {"types", "[resource-drop]item", "i:", &dropItem},
    };

    // one engine and linker for all plugins, one store each: shared host functions, no shared state
    class Runtime {
    public:
        static Runtime &get() {
            static Runtime runtime;
            return runtime;
        }

        ~Runtime() {
            if (mLinker != nullptr)
                wasmtime_linker_delete(mLinker);
            if (mEngine != nullptr)
                wasm_engine_delete(mEngine);
        }

        bool ready(std::string &error) {
            if (mLinker != nullptr)
                return true;
            if (!mFailure.empty()) {
                error = mFailure;
                return false;
            }

            wasm_config_t *config = wasm_config_new();
            wasmtime_config_consume_fuel_set(config, true);
            wasmtime_config_wasm_threads_set(config, false);
            mEngine = wasm_engine_new_with_config(config);
            if (mEngine == nullptr) {
                mFailure = error = "the Wasm engine could not be created";
                return false;
            }

            mLinker = wasmtime_linker_new(mEngine);
            for (const WasmImports::Definition &definition: WasmImports::all()) {
                if (!_define(definition, error))
                    return false;
            }
            for (const WasmImports::Definition &definition: RESOURCE_DROPS) {
                if (!_define(definition, error))
                    return false;
            }
            return true;
        }

        wasm_engine_t *getEngine() {
            return mEngine;
        }

        const wasmtime_linker_t &getLinker() {
            return *mLinker;
        }

    private:
        bool _define(const WasmImports::Definition &definition, std::string &error) {
            const std::string module = std::string(PACKAGE_PREFIX) + definition.mInterface + PACKAGE_SUFFIX;
            wasm_functype_t *type = functionType(definition.mSignature);
            wasmtime_error_t *failure = wasmtime_linker_define_func_unchecked(
                    mLinker, module.data(), module.size(), definition.mName, std::strlen(definition.mName), type,
                    &invoke, const_cast<WasmImports::Definition *>(&definition), nullptr);
            wasm_functype_delete(type);
            if (failure == nullptr)
                return true;

            mFailure = error = "could not define " + module + "#" + definition.mName + ": " + errorText(failure);
            wasmtime_linker_delete(mLinker);
            mLinker = nullptr;
            return false;
        }

        wasm_engine_t *mEngine = nullptr;
        wasmtime_linker_t *mLinker = nullptr;
        std::string mFailure;
    };

    WasmPlugin &instanceOf(void *userData) {
        return *static_cast<WasmPlugin *>(userData);
    }

    void onLoad(void *userData) {
        instanceOf(userData).call(WasmPlugin::Export::Load, nullptr, 0);
    }

    int onEnable(void *userData) {
        WasmPlugin &plugin = instanceOf(userData);
        if (!plugin.hasExport(WasmPlugin::Export::Enable))
            return plugin.isFaulted() ? 0 : 1;

        wasmtime_val_raw_t values[1];
        return plugin.call(WasmPlugin::Export::Enable, values, 1) && values[0].i32 != 0 ? 1 : 0;
    }

    void onDisable(void *userData) {
        instanceOf(userData).call(WasmPlugin::Export::Disable, nullptr, 0);
    }

    bool readModule(const fs::path &path, std::vector<uint8_t> &bytes) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return !file.bad();
    }
}

bool WasmHost::load(LoadedPlugin &plugin) {
    const std::string &name = plugin.mDescription.mName;
    const fs::path path = fs::path(plugin.mDirectory) / plugin.mDescription.mMain;

    std::vector<uint8_t> bytes;
    if (!readModule(path, bytes)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s cannot be read", name.c_str(),
                  path.string().c_str());
        return false;
    }

    Runtime &runtime = Runtime::get();
    std::string error;
    if (!runtime.ready(error)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s", name.c_str(), error.c_str());
        return false;
    }

    wasmtime_module_t *module = nullptr;
    wasmtime_error_t *failure = wasmtime_module_new(runtime.getEngine(), bytes.data(), bytes.size(), &module);
    if (failure != nullptr) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s is not a valid Wasm module: %s", name.c_str(),
                  path.string().c_str(), errorText(failure).c_str());
        return false;
    }

    const PropertiesSettings &properties = PluginManager::getInstance().getOwner().getProperties();
    wasmtime_store_t *store = wasmtime_store_new(runtime.getEngine(), nullptr, nullptr);
    // -1 means unlimited: tables need finite caps or table.grow escapes the memory limit
    wasmtime_store_limiter(store, (int64_t) properties.getWasmPluginMemoryMb() * 1024 * 1024, MAX_TABLE_ELEMENTS, 1,
                           MAX_TABLES, 1);

    auto instance = std::make_shared<WasmPlugin>(plugin, store, (uint64_t) properties.getWasmPluginFuel());
    const bool instantiated = instance->instantiate(runtime.getLinker(), *module, error);
    wasmtime_module_delete(module);
    if (!instantiated) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s", name.c_str(), error.c_str());
        return false;
    }

    plugin.mCallbacks.onLoad = &onLoad;
    plugin.mCallbacks.onEnable = &onEnable;
    plugin.mCallbacks.onDisable = &onDisable;
    plugin.mCallbacks.userData = instance.get();
    plugin.mInstance = instance;

    LOG_INFO(LogAreaID::Server, "Loaded Wasm plugin %s %s", name.c_str(), plugin.mDescription.mVersion.c_str());
    return true;
}
