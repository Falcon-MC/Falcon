#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

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

    BlockState vanilla(const char *identifier) {
        return VanillaBlocks::fromIdentifier(identifier)->toBlockState();
    }

    // a double chest moved onto itself, then arguments whose conversion replaces the block under the view
    const char *const SCRIPT = R"JS(
import { world, system, CloneMode } from "@minecraft/server";

const r = {};
const capture = (f) => {
    try {
        f();
        return "ok";
    } catch (e) {
        return e.name;
    }
};
const describe = (c, slots) => slots.map((i) => {
    const item = c.getItem(i);
    return item ? item.typeId.replace("minecraft:", "") + item.amount : "-";
}).join(",");

system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "views:run")
        return;
    const d = world.getDimension("overworld");
    const stone = { x: 6, y: 100, z: 0 };
    const replace = (at) => d.cloneBlocks(stone, stone, at, CloneMode.ForceCopy);

    const double = d.getBlock({ x: 0, y: 100, z: 0 }).getComponent("minecraft:inventory").container;
    r.size = double.size;
    double.swapItems(0, 30, double);
    double.moveItem(1, 40, double);
    r.transfer = capture(() => double.transferItem(2, double));
    r.double = describe(double, [0, 1, 2, 30, 40]);

    const single = d.getBlock({ x: 4, y: 100, z: 0 }).getComponent("minecraft:inventory").container;
    r.broken = capture(() => single.getItem({ valueOf() { replace({ x: 4, y: 100, z: 0 }); return 0; } }));
    r.valid = single.isValid;

    const sign = d.getBlock({ x: 8, y: 100, z: 0 }).getComponent("minecraft:sign");
    r.sign = capture(() => sign.setText({ get rawtext() { replace({ x: 8, y: 100, z: 0 }); return [{ text: "x" }]; } }));
    world.setDynamicProperty("views", JSON.stringify(r));
});
)JS";

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writeFile(packs / "views" / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"views\","
                  " \"uuid\": \"00000000-0000-4000-8000-600000000002\", \"version\": [1, 0, 0],"
                  " \"min_engine_version\": [1, 21, 0]}, \"modules\": [{\"type\": \"script\","
                  " \"language\": \"javascript\", \"uuid\": \"00000000-0000-4000-9000-600000000002\","
                  " \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}], \"dependencies\":"
                  " [{\"module_name\": \"@minecraft/server\", \"version\": \"2.11.0\"}]}\n");
        writeFile(packs / "views" / "scripts" / "main.js", SCRIPT);

        ServerNetworkHandler owner("Script container views", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the views pack did not start");
        engine.startup();

        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        for (int32_t x = 0; x <= 8; ++x)
            level.setBlock(Vector3i(x, 99, 0), vanilla("minecraft:stone"), false);
        level.setBlock(Vector3i(0, 100, 0), vanilla("minecraft:chest"), false);
        level.setBlock(Vector3i(1, 100, 0), vanilla("minecraft:chest"), false);
        level.setBlock(Vector3i(4, 100, 0), vanilla("minecraft:chest"), false);
        level.setBlock(Vector3i(6, 100, 0), vanilla("minecraft:stone"), false);
        level.setBlock(Vector3i(8, 100, 0), vanilla("minecraft:standing_sign"), false);
        ChestBlockActor &left = level.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(0, 100, 0));
        ChestBlockActor &right = level.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(1, 100, 0));
        left.pairWith(right);
        Container *pair = left.getContainer();
        require(pair != nullptr && pair->getContainerSize() == 54, "the chests did not pair");
        require(left.getContainer() == pair, "a paired chest rebuilt its double container");
        pair->setContainerItem(0, owner.createItemStack("minecraft:diamond", 1));
        pair->setContainerItem(1, owner.createItemStack("minecraft:stone", 5));
        pair->setContainerItem(2, owner.createItemStack("minecraft:dirt", 3));
        level.getBlockActors()
                .getOrCreate<ChestBlockActor>(Vector3i(4, 100, 0))
                .getContainer()
                ->setContainerItem(0, owner.createItemStack("minecraft:apple", 2));
        level.getBlockActors().getOrCreate<SignBlockActor>(Vector3i(8, 100, 0));

        engine.tick(0);
        engine.tick(1);
        engine.emitScriptEvent("views:run", "");

        const auto it = owner.getWorldDynamicProperties().find("views");
        require(it != owner.getWorldDynamicProperties().end(), "the views pack wrote no result");
        const std::string wanted = "{\"size\":54,\"transfer\":\"ok\",\"double\":\"dirt3,-,-,diamond1,stone5\","
                                   "\"broken\":\"InvalidContainerError\",\"valid\":false,\"sign\":\"ok\"}";
        require(it->second.mString == wanted, "container views: expected " + wanted + ", got " + it->second.mString);
        require(level.getBlockActors().find<ChestBlockActor>(Vector3i(4, 100, 0)) == nullptr,
                "the replaced chest kept its block actor");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-views-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script container view regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
