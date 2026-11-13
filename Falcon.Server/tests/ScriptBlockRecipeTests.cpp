#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

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

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-600000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-600000000001\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const DAMPENER_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": { "identifier": "test:dampener" },
    "components": { "minecraft:vibration_properties": { "can_dampen_vibrations": true } }
  }
})JSON";

    const char *const SCRIPT = R"JS(
import { world, system, ItemStack, BlockRecipeProcessingComponent, BlockVibrationPropertiesComponent }
    from "@minecraft/server";

const r = {};
const save = () => world.setDynamicProperty("recipes", JSON.stringify(r));
const fail = (f) => {
    try { f(); return "ok"; } catch (e) {
        return e.name + ":" + e.message + (e.value !== undefined ? ":" + e.minValue + "-" + e.maxValue + "@" + e.value : "");
    }
};
const at = (x, z) => world.getDimension("overworld").getBlock({ x, y: 100, z });
let kept;

system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id === "recipes:run") {
        const crafter = at(5, 5);
        const processing = crafter.getComponent("recipe_processing");
        kept = processing;
        r.processing = (processing instanceof BlockRecipeProcessingComponent) + ":" + processing.typeId + ":"
            + BlockRecipeProcessingComponent.componentId + ":" + processing.inputSlotCount;
        r.emptyOutput = String(processing.getOutputItem());
        processing.setInputItem(0, new ItemStack("minecraft:oak_log", 1));
        const output = processing.getOutputItem();
        r.output = output.typeId + "x" + output.amount;
        r.inputs = processing.getInputItem(0).typeId + ":" + String(processing.getInputItem(1));
        r.slots = processing.getInputSlotEnabled(1) + ":";
        processing.setInputSlotEnabled(1, false);
        processing.setInputSlotEnabled(0, false);
        r.slots += processing.getInputSlotEnabled(1) + ":" + processing.getInputSlotEnabled(0);
        processing.setInputItem(0);
        r.cleared = String(processing.getInputItem(0));
        r.errors = [fail(() => processing.getInputItem(9)), fail(() => processing.setInputItem(12)),
            fail(() => processing.getInputSlotEnabled(9)), fail(() => processing.setInputSlotEnabled(9, true)),
            fail(() => processing.getInputItem("a")), fail(() => processing.setInputItem(0, 5))].join("|");
        r.stoneProcessing = String(at(6, 6).getComponent("recipe_processing"));

        const stone = at(6, 6).getComponent("vibration_properties");
        const dampener = at(7, 7).getComponent("minecraft:vibration_properties");
        r.vibration = (stone instanceof BlockVibrationPropertiesComponent) + ":" + stone.getCanDampenVibrations() + ":"
            + stone.getCanOccludeVibrations() + ":" + dampener.getCanDampenVibrations() + ":"
            + dampener.getCanOccludeVibrations() + ":" + BlockVibrationPropertiesComponent.componentId;
        r.vibrationListed = at(7, 7).getComponents().some((c) => c.typeId === "minecraft:vibration_properties");
        save();
    } else if (event.id === "recipes:gone") {
        r.gone = fail(() => kept.getOutputItem());
        save();
    }
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("recipes");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the recipe pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the recipe pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    // without upcoming_creator_features the game rejects a block using minecraft:vibration_properties
    void checkVibrationNeedsExperiment(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "gated";
        writePack(packs, "gated", "");
        writeFile(packs / "gated" / "blocks" / "dampener.json", DAMPENER_BLOCK);
        ServerNetworkHandler probe("Script block recipe gate", "", 1);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry &registry = CustomContentRegistry::getInstance();
        registry.reset();
        registry.setUpcomingCreatorFeatures(false);
        registry.load(manager, probe.getItemDefinitions(), probe.getBlockDefinitions());
        require(registry.findBlock("test:dampener") == nullptr,
                "a vibration_properties block loaded without the experiment");
        registry.reset();
        registry.setUpcomingCreatorFeatures(true);
        registry.load(manager, probe.getItemDefinitions(), probe.getBlockDefinitions());
        const CustomBlockDefinition *dampener = registry.findBlock("test:dampener");
        require(dampener != nullptr && dampener->mCanDampenVibrations && !dampener->mCanOccludeVibrations,
                "can_dampen_vibrations and the default can_occlude_vibrations were not read");
        registry.reset();
    }

    void run(const std::filesystem::path &directory) {
        checkVibrationNeedsExperiment(directory);

        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "recipes", SCRIPT);
        writeFile(packs / "recipes" / "blocks" / "dampener.json", DAMPENER_BLOCK);
        CustomContentRegistry::getInstance().setUpcomingCreatorFeatures(true);

        ServerNetworkHandler owner("Script block recipe", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the recipe pack did not start");
        engine.startup();

        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i crafterAt(5, 100, 5);
        level.setBlock(crafterAt, BlockState("minecraft:crafter"), false);
        CrafterBlockActor &crafter = level.getBlockActors().getOrCreate<CrafterBlockActor>(crafterAt);
        level.setBlock(Vector3i(6, 100, 6), BlockState("minecraft:stone"), false);
        level.setBlock(Vector3i(7, 100, 7), BlockState("test:dampener"), false);
        // the server does not listen here: its own tick() would return at once
        engine.tick(0);

        engine.emitScriptEvent("recipes:run", "");
        engine.tick(1);
        // the disabled bits land on the block actor, even the one of a filled slot
        require((crafter.getInventory().getDisabledSlots() & 0x3) == 0x3,
                "setInputSlotEnabled did not reach the crafter");

        level.getBlockActors().remove(crafterAt);
        level.setBlock(crafterAt, BlockState("minecraft:stone"), false);
        engine.emitScriptEvent("recipes:gone", "");
        engine.tick(2);

        const std::unique_ptr<json::Value> r = result(owner);
        expect(*r, "processing", "true:minecraft:recipe_processing:minecraft:recipe_processing:9");
        expect(*r, "emptyOutput", "undefined");
        expect(*r, "output", "minecraft:oak_planksx4");
        expect(*r, "inputs", "minecraft:oak_log:undefined");
        expect(*r, "slots", "true:false:false");
        expect(*r, "cleared", "undefined");
        expect(*r, "errors",
               "ArgumentOutOfBoundsError:getInputItem:0-9@9|ArgumentOutOfBoundsError:setInputItem:0-9@12|"
               "ArgumentOutOfBoundsError:getInputSlotDisabled:0-9@9|"
               "ArgumentOutOfBoundsError:setInputSlotDisabled:0-9@9|"
               "TypeError:Native type conversion failed.|TypeError:Native type conversion failed.");
        expect(*r, "stoneProcessing", "undefined");
        expect(*r, "vibration", "true:false:false:true:false:minecraft:vibration_properties");
        expect(*r, "vibrationListed", "true");
        expect(*r, "gone",
               "InvalidBlockComponentError:Attempting to access invalid block component minecraft:recipe_processing.");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-block-recipe-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script block recipe regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
