#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    const char *const DIAGNOSTICS = R"JS(
import { world, system } from "@minecraft/server";
import { sentry, Sentry, SentryEventLevel, SentryUninitializedError, SentryAlreadyInitializedError }
    from "@minecraft/diagnostics";

const r = {};
const errorOf = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
r.uninitialized = errorOf(() => sentry.getTags());
r.badDsn = errorOf(() => sentry.init({ dsn: "not a dsn" }));
r.init = errorOf(() => sentry.init({ dsn: "http://key@127.0.0.1:1/42", maxBreadcrumbs: 1 }));
r.again = errorOf(() => sentry.init({ dsn: "http://key@127.0.0.1:1/42" }));
sentry.addTag("team", "blue");
sentry.addTag("gone", "x");
sentry.removeTag("gone");
const tags = sentry.getTags();
r.tags = tags.team + "|" + (tags.gone === undefined) + "|" + tags.module_name + "|" + tags.minecraft_version;
r.crumb = errorOf(() => { sentry.addBreadcrumb(SentryEventLevel.info, "one", "cat"); sentry.addBreadcrumb("debug", "two"); });
r.capture = errorOf(() => sentry.captureException(new Error("boom"), { level: "warning", tags: { a: "b" } }));
r.badLevel = errorOf(() => sentry.addBreadcrumb("loud", "x"));
r.classes = (sentry instanceof Sentry) + "|" + typeof SentryUninitializedError + "|"
    + typeof SentryAlreadyInitializedError + "|" + errorOf(() => new Sentry());
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id === "diag:report")
        world.setDynamicProperty("diagnostics", JSON.stringify(r));
});
)JS";

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        require(value->mString == wanted,
                std::string(key) + ": expected \"" + wanted + "\", got \"" + value->mString + "\"");
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path pack = directory / "packs" / "diagnostics";
        writeFile(pack / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"diagnostics\", \"uuid\": "
                  "\"00000000-0000-4000-8000-600000000001\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, "
                  "0]}, \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": "
                  "\"00000000-0000-4000-9000-600000000001\", \"entry\": \"scripts/main.js\", \"version\": [2, 1, 0]}], "
                  "\"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"2.12.0-beta\"}, "
                  "{\"module_name\": \"@minecraft/diagnostics\", \"version\": \"1.0.0-beta\"}, "
                  "{\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}]}\n");
        writeFile(pack / "scripts" / "main.js", DIAGNOSTICS);

        ServerNetworkHandler owner("Script diagnostics", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover((directory / "packs").string());
        require(engine.loadPacks(manager.getPacks()) == 1, "the diagnostics pack did not start");
        engine.startup();
        engine.tick(1);
        engine.queueScriptEvent("diag:report", "");
        engine.tick(2);

        const auto it = owner.getWorldDynamicProperties().find("diagnostics");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the diagnostics pack");
        const std::unique_ptr<json::Value> report = json::parse(it->second.mString);
        require(report != nullptr && report->isObject(), "the diagnostics pack wrote no JSON");
        expect(*report, "uninitialized", "SentryUninitializedError:Sentry is not initialized");
        expect(*report, "badDsn",
               "InvalidArgumentError:Invalid value passed. DSN format is invalid, see Sentry documentation for "
               "correct DSN format.");
        expect(*report, "init", "ok");
        expect(*report, "again", "SentryAlreadyInitializedError:Sentry is already initialized");
        expect(*report, "tags", "blue|true|diagnostics|1.26.60");
        expect(*report, "crumb", "ok");
        expect(*report, "capture", "ok");
        expect(*report, "badLevel", "TypeError:Native type conversion failed.");
        expect(*report, "classes", "true|function|function|ReferenceError:No constructor for native class 'Sentry'.");
        engine.shutdown();
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-diagnostics-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        writeFile(directory / "config" / "default" / "permissions.json", R"({"allowed_modules": ["*"]})");
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script diagnostics regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
