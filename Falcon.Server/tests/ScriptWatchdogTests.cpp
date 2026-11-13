#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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

    // the handler runs restricted: it can only schedule the write
    const char *const HUNG = R"JS(
import { world, system, WatchdogTerminateBeforeEvent } from "@minecraft/server";

const r = { events: [], after: 0 };
const save = () => world.setDynamicProperty("hung", JSON.stringify(r));
system.beforeEvents.watchdogTerminate.subscribe((event) => {
    r.events.push(event.terminateReason + ":" + event.cancel + ":" + (event instanceof WatchdogTerminateBeforeEvent));
    event.cancel = true;
    system.run(save);
});
system.runTimeout(() => { for (;;) {} }, 1);
system.runTimeout(() => { r.after++; save(); }, 3);
)JS";

    const char *const BYSTANDER = R"JS(
import { world, system } from "@minecraft/server";

const r = { events: 0, ticks: 0 };
system.beforeEvents.watchdogTerminate.subscribe(() => r.events++);
system.runInterval(() => { r.ticks++; world.setDynamicProperty("bystander", JSON.stringify(r)); }, 1);
)JS";

    void writePack(const std::filesystem::path &packs, const std::string &name, const std::string &index,
                   const char *script) {
        writeFile(packs / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-7000000000" + index
                          + "\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, 0]}, \"modules\":"
                            " [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-7000000000"
                          + index
                          + "\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}], \"dependencies\":"
                            " [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(packs / name / "scripts" / "main.js", script);
    }

    std::string result(ServerNetworkHandler &owner, const std::string &key) {
        const auto it = owner.getWorldDynamicProperties().find(key);
        require(it != owner.getWorldDynamicProperties().end(), "no result for " + key);
        return it->second.mString;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "hung", "01", HUNG);
        writePack(packs, "bystander", "02", BYSTANDER);

        ServerNetworkHandler owner("Script watchdog", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        engine.setWatchdogMs(200);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 2, "the watchdog packs did not start");
        engine.startup();

        for (int64_t tick = 0; tick <= 5; ++tick)
            engine.tick(tick);

        // one event for the hang, in the hung pack only; both packs keep running
        const std::string hung = result(owner, "hung");
        require(hung == "{\"events\":[\"Hang:false:true\"],\"after\":1}", "hung pack: got " + hung);
        const std::string bystander = result(owner, "bystander");
        require(bystander.compare(0, 12, "{\"events\":0,") == 0, "bystander pack: got " + bystander);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-watchdog-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script watchdog regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
