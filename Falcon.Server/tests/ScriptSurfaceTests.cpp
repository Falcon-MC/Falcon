#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ILogEndPoint.h"
#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Binding/ScriptSurface.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
#include "../src/Scripting/Binding/ScriptSurface.inc"

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    std::string quoted(const std::string &text) {
        std::string out = "\"";
        for (char c: text) {
            if (c == '"' || c == '\\')
                out += '\\';
            out += c;
        }
        return out + "\"";
    }

    // the only official names allowed to stay unbound; each needs its reason
    bool excepted(const std::string &name) {
        // aim assist reads its presets and categories from DataGen data, whose publish workflow is off for now
        return name.find("AimAssist") != std::string::npos;
    }

    // every server name the version declares and the table binds
    std::vector<std::string> declared(const ScriptModuleVersion &version) {
        const ScriptSurface surface("@minecraft/server", version);
        std::vector<std::string> names;
        for (const ScriptSurfaceEntry &entry: SCRIPT_SURFACE) {
            const std::string name = entry.mName;
            if (name.compare(0, 7, "server:") != 0 || !surface.has(name.substr(7)))
                continue;
            names.push_back(name.substr(7));
        }
        return names;
    }

    // members of classes the game hands out as plain payloads: checked on a live instance by the api tests
    const char *const CHECK = R"JS(
import * as server from "@minecraft/server";
const missing = [];
// signal holders carry their signals as own properties
const holders = {
    WorldAfterEvents: () => server.world.afterEvents,
    WorldBeforeEvents: () => server.world.beforeEvents,
    SystemAfterEvents: () => server.system.afterEvents,
    SystemBeforeEvents: () => server.system.beforeEvents,
};
const instanceOnly = (cls) => /(Event|Error|Result|Options)$/.test(cls)
    || cls === "CustomComponentParameters" || cls === "CustomCommandOrigin";
for (const name of NAMES) {
    const dot = name.indexOf(".");
    const head = dot < 0 ? name : name.slice(0, dot);
    const target = server[head];
    if (target === undefined) {
        missing.push(name);
        continue;
    }
    if (dot < 0)
        continue;
    const rest = name.slice(dot + 1);
    const equals = rest.indexOf("=");
    if (equals >= 0) {
        if (String(target[rest.slice(0, equals)]) !== rest.slice(equals + 1))
            missing.push(name);
        continue;
    }
    if (rest === "constructor")
        continue;
    if (rest.startsWith("static ")) {
        if (!(rest.slice(7) in target))
            missing.push(name);
        continue;
    }
    if (typeof target === "function" && target.prototype !== undefined && rest in target.prototype)
        continue;
    // 1.3 exports WorldAfterEvents without World.afterEvents: nothing reaches its signals
    if (holders[head] !== undefined && (holders[head]() === undefined || rest in holders[head]()))
        continue;
    // the world and system singletons carry some members themselves
    if ((head === "World" && rest in server.world) || (head === "System" && rest in server.system))
        continue;
    if (instanceOnly(head))
        continue;
    missing.push(name);
}
console.warn("surface:" + JSON.stringify(missing));
)JS";

    // the pack reports through the content log, written on the log thread: early 1.x versions have no dynamic
    // properties
    class ReportEndPoint final : public ILogEndPoint {
    public:
        void log(const LogDetails &details) override {
            std::lock_guard<std::mutex> lock(mMutex);
            if (std::getenv("SURFACE_LOG") != nullptr)
                std::clog << details.mMessage.substr(0, 400) << "\n";
            const size_t at = details.mMessage.find("surface:");
            if (at != std::string::npos)
                mReport = details.mMessage.substr(at + 8);
        }

        std::string take() {
            std::lock_guard<std::mutex> lock(mMutex);
            return mReport;
        }

    private:
        std::mutex mMutex;
        std::string mReport;
    };

    std::vector<ScriptModuleVersion> versions() {
        std::vector<ScriptModuleVersion> all;
        for (int minor = 0; minor <= 19; ++minor)
            all.push_back({"@minecraft/server", "1." + std::to_string(minor) + ".0", ScriptApiTrack::Stable1, minor});
        for (int minor = 0; minor <= 11; ++minor)
            all.push_back({"@minecraft/server", "2." + std::to_string(minor) + ".0", ScriptApiTrack::Stable2, minor});
        return all;
    }

    std::vector<std::string> missingIn(const std::filesystem::path &directory, const ScriptModuleVersion &version) {
        const std::filesystem::path packs = directory / ("packs-" + version.mVersion);
        std::string names = "const NAMES = [";
        for (const std::string &name: declared(version))
            names += quoted(name) + ",";
        names += "];\n";
        writeFile(packs / "surface" / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"surface\", \"uuid\": "
                  "\"00000000-0000-4000-8000-000000000001\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, "
                  "0]}, \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": "
                  "\"00000000-0000-4000-9000-000000000001\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}], "
                  "\"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \""
                          + version.mVersion + "\"}]}");
        // imports first: the names follow them
        std::string script = CHECK;
        const size_t body = script.find("const missing");
        script.insert(body, names);
        writeFile(packs / "surface" / "scripts" / "main.js", script);

        auto report = std::make_shared<ReportEndPoint>();
        BedrockLog::addEndPoint(report);
        ServerNetworkHandler owner("Script surface", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        if (engine.loadPacks(manager.getPacks()) != 1)
            throw std::runtime_error(version.mVersion + ": the pack did not start");
        engine.startup();
        std::string text;
        for (int64_t tick = 0; tick < 3; ++tick)
            engine.tick(tick);
        BedrockLog::flush();
        for (int attempt = 0; attempt < 200 && (text = report->take()).empty(); ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (text.empty())
            throw std::runtime_error(version.mVersion + ": the pack wrote no result");
        const std::unique_ptr<json::Value> parsed = json::parse(text);
        std::vector<std::string> missing;
        for (const std::unique_ptr<json::Value> &value: parsed->mArray)
            missing.push_back(value->string());
        engine.shutdown();
        BedrockLog::removeEndPoint(report);
        CustomContentRegistry::getInstance().reset();
        return missing;
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-surface-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        for (const ScriptUnboundEntry &entry: SCRIPT_UNBOUND) {
            if (excepted(entry.mName))
                continue;
            std::clog << entry.mName << " is official but outside the bound set\n";
            result = 1;
        }
        for (const ScriptModuleVersion &version: versions()) {
            if (std::getenv("SURFACE_ONLY") != nullptr && version.mVersion != std::getenv("SURFACE_ONLY"))
                continue;
            for (const std::string &name: missingIn(directory, version)) {
                if (excepted(name))
                    continue;
                std::clog << version.mVersion << ": " << name << " is declared but not bound\n";
                result = 1;
            }
        }
    } catch (const std::exception &error) {
        std::clog << "Script surface check failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
