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

    // the closures below are referenced only by the scheduler or the signal: removing them mid-call used to free them
    const char *const SCRIPT = R"JS(
import { world, system } from "@minecraft/server";

const r = { self: 0, selfAfter: "", victim: 0, killer: 0, first: 0, second: 0, third: 0 };
const save = () => world.setDynamicProperty("lifetime", JSON.stringify(r));

const suffix = "-cleared";
const self = system.runInterval(() => {
    r.self++;
    system.clearRun(self);
    r.selfAfter = ("self" + suffix).toUpperCase();
    save();
}, 1);

let victim = 0;
system.runInterval(() => {
    r.killer++;
    system.clearRun(victim);
    save();
}, 2);
victim = system.runInterval(() => { r.victim++; save(); }, 2);

const signal = system.afterEvents.scriptEventReceive;
let second = signal.subscribe(() => { r.second++; save(); });
const first = signal.subscribe((event) => {
    if (event.id !== "lifetime:emit")
        return;
    r.first++;
    if (second !== undefined) {
        signal.unsubscribe(second);
        second = undefined;
        signal.subscribe(() => { r.third++; save(); });
    }
    save();
});
signal.unsubscribe(second);
second = signal.subscribe(() => { r.second++; save(); });
)JS";

    std::string result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("lifetime");
        require(it != owner.getWorldDynamicProperties().end(), "the pack wrote no result");
        return it->second.mString;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writeFile(packs / "lifetime" / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"lifetime\","
                  " \"uuid\": \"00000000-0000-4000-8000-600000000001\", \"version\": [1, 0, 0],"
                  " \"min_engine_version\": [1, 21, 0]}, \"modules\": [{\"type\": \"script\","
                  " \"language\": \"javascript\", \"uuid\": \"00000000-0000-4000-9000-600000000001\","
                  " \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}], \"dependencies\":"
                  " [{\"module_name\": \"@minecraft/server\", \"version\": \"2.0.0\"}]}\n");
        writeFile(packs / "lifetime" / "scripts" / "main.js", SCRIPT);

        ServerNetworkHandler owner("Script callback lifetime", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the lifetime pack did not start");
        engine.startup();

        for (int64_t tick = 0; tick <= 6; ++tick)
            engine.tick(tick);

        // first drops second, queued behind it; only the next emission reaches the handler added meanwhile
        engine.emitScriptEvent("lifetime:emit", "");
        engine.emitScriptEvent("lifetime:emit", "");

        const std::string head = "{\"self\":1,\"selfAfter\":\"SELF-CLEARED\",\"victim\":0,\"killer\":3,\"first\":2,";
        const std::string tail = "\"third\":1}";
        const std::string got = result(owner);
        require(got == head + "\"second\":0," + tail || got == head + "\"second\":1," + tail,
                "callback lifetime: got " + got);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-lifetime-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script callback lifetime regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
