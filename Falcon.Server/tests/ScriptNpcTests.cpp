#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/NpcActor.h"
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
                          + "\", \"uuid\": \"00000000-0000-4000-8000-400000000071\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-400000000071\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const NPC = R"JS(
import { world, system, EntityComponentTypes, EntityNpcComponent } from "@minecraft/server";

const r = {};
const save = () => world.setDynamicProperty("npc", JSON.stringify(r));
const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };

system.afterEvents.scriptEventReceive.subscribe((event) => {
    const entities = world.getDimension("overworld").getEntities();
    const npc = entities.find((e) => e.typeId === "minecraft:npc");
    const cow = entities.find((e) => e.typeId === "minecraft:cow");
    if (event.id === "npc:read") {
        const c = npc.getComponent("minecraft:npc");
        r.read = [EntityNpcComponent.componentId, EntityComponentTypes.Npc, c instanceof EntityNpcComponent,
            c.typeId, JSON.stringify(c.name), JSON.stringify(c.defaultScene), c.skinIndex,
            cow.getComponent("npc") === undefined, npc.hasComponent("minecraft:npc")].join("|");
        c.skinIndex = 3;
        c.name = "Guide";
        c.defaultScene = "intro";
        r.written = [c.skinIndex, c.name, c.defaultScene].join("|");
        r.badIndex = fail(() => { c.skinIndex = 60; });
        r.sameIndex = fail(() => { c.skinIndex = 3; });
        r.negative = fail(() => { c.skinIndex = -1; });
        r.badName = fail(() => { c.name = 4; });
        c.name = JSON.stringify({ rawtext: [{ text: "Sir " }, { text: "Bob" }] });
        r.raw = c.name;
    } else if (event.id === "npc:long") {
        npc.getComponent("minecraft:npc").name = "abcdefghijklmnopqrstuvwxyz0123456789";
    }
    save();
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("npc");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the npc pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the npc pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "npc", NPC);

        ServerNetworkHandler owner("Script npc", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the npc pack did not start");

        engine.startup();
        EntityDefinitions::initialize();
        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        engine.tick(0);

        ServerActor *spawned = owner.spawnActor(level, "minecraft:npc", Vector3f(0.5f, 100.0f, 0.5f));
        NpcActor *npc = dynamic_cast<NpcActor *>(spawned);
        require(npc != nullptr, "minecraft:npc did not spawn an NpcActor");
        require(npc->getSkins().size() == 60, "the npc skin list is not the definition's sixty skins");
        require(npc->getSkins()[20].mVariant == 25, "the npc skins are out of the definition's order");
        require(owner.spawnActor(level, "minecraft:cow", Vector3f(2.5f, 100.0f, 0.5f)) != nullptr, "no fixture cow");

        engine.emitScriptEvent("npc:read", "");
        std::unique_ptr<json::Value> r = result(owner);
        expect(*r, "read", "minecraft:npc|minecraft:npc|true|minecraft:npc|\"\"|\"\"|0|true|true");
        expect(*r, "written", "3|Guide|intro");
        expect(*r, "badIndex",
               "ArgumentOutOfBoundsError:Unsupported or out of bounds value passed to function argument [0]: "
               "Invalid skin index, Value: 60, Argument bounds: [0, 59]");
        expect(*r, "sameIndex",
               "ArgumentOutOfBoundsError:Unsupported or out of bounds value passed to function argument [0]: "
               "Invalid skin index, Value: 3, Argument bounds: [0, 59]");
        expect(*r, "negative",
               "ArgumentOutOfBoundsError:Unsupported or out of bounds value passed to function argument [0]: "
               "Invalid skin index, Value: -1, Argument bounds: [0, 59]");
        expect(*r, "badName", "TypeError:Native type conversion failed.");
        expect(*r, "raw", "{\"rawtext\":[{\"text\":\"Sir \"},{\"text\":\"Bob\"}]}");
        require(npc->getSkinIndex() == 3, "the skin index did not reach the actor");
        require(npc->getNameTag() == "Sir Bob", "the raw text name did not resolve into the name tag");
        require(npc->getSceneName() == "intro", "the scene did not reach the actor");

        const Tag saved = npc->saveNbt();
        require(saved.getString("RawtextName") == npc->getRawName() && saved.getString("SceneName") == "intro"
                        && saved.getInt("Variant") == 3,
                "the npc data was not saved under the game's keys");
        NpcActor reloaded(npc->getRuntimeId() + 1000, NpcActor::IDENTIFIER);
        reloaded.loadNbt(saved);
        require(reloaded.getSkinIndex() == 3 && reloaded.getRawName() == npc->getRawName()
                        && reloaded.getSceneName() == "intro",
                "the npc data did not load back");

        engine.emitScriptEvent("npc:long", "");
        require(npc->getNameTag() == "abcdefghijklmnopqrstuvwxyz012345", "the name tag was not cut at 32 code points");
        require(NpcActor::resolveName("plain") == "plain", "a plain name did not stay as it is");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-npc-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script npc regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
