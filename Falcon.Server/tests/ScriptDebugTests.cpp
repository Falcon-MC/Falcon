#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PrimitiveShapesPacket.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
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
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &dependencies,
                   const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-400000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-400000000001\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": ["
                          + dependencies + "]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const DEBUG = R"JS(
import { world, system } from "@minecraft/server";
import { DebugArrow, DebugBox, DebugCylinder, DebugShape, DebugText, DebugDrawer, DiagnosticsError,
    DiagnosticsErrorReason, DiagnosticsChartDisplayType, collectPluginStats, collectRuntimeStats,
    disableWatchdogTimingWarnings, debugDrawer, diagnosticsManager } from "@minecraft/debug-utilities";

const r = {};
const errorOf = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
r.early = errorOf(() => new DebugBox({ x: 0, y: 0, z: 0 }));
const shapes = {};
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id === "debug:add") {
        const overworld = world.getDimension("overworld");
        const box = new DebugBox({ x: 1, y: 2, z: 3, dimension: overworld });
        box.color = { red: 1, green: 0, blue: 0, alpha: 1 };
        box.bound = { x: 2, y: 1, z: 1 };
        box.timeLeft = 0.12;
        r.box = box.scale + "|" + box.dimension.id + "|" + box.hasDuration + "|" + box.totalTimeLeft + "|"
            + JSON.stringify(box.location) + "|" + (box instanceof DebugShape);
        const arrow = new DebugArrow({ x: 0, y: 0, z: 0 }, { x: 0, y: 5, z: 0 });
        r.arrow = arrow.headLength + "|" + arrow.headRadius + "|" + arrow.headSegments + "|"
            + JSON.stringify(arrow.endLocation) + "|" + (arrow.dimension === undefined);
        r.segments = errorOf(() => { arrow.headSegments = 2; });
        r.segmentsError = (() => { try { arrow.headSegments = 200; } catch (e) {
            return e.minValue + "|" + e.maxValue + "|" + e.value; } })();
        r.distance = errorOf(() => { arrow.maximumRenderDistance = -1; });
        const cylinder = new DebugCylinder({ x: 4, y: 4, z: 4 });
        cylinder.radii = { x: 1.5, y: 0.5 };
        const text = new DebugText({ x: 0, y: 1, z: 0 }, { rawtext: [{ text: "hi" }] });
        r.text = JSON.stringify(text.text) + "|" + text.backfaceVisible + "|" + text.textBackfaceVisible;
        text.setText("plain");
        r.hidden = new DebugText({ x: 0, y: 0, z: 0 }, "x");
        r.hidden.defaultVisibleToAll = false;
        r.constructor = errorOf(() => new DebugShape());
        r.drawer = errorOf(() => new DebugDrawer());
        r.badLocation = errorOf(() => new DebugBox({ x: "a", y: 0, z: 0 }));
        debugDrawer.addShape(box);
        debugDrawer.addShape(arrow, overworld);
        debugDrawer.addShape(cylinder);
        debugDrawer.addShape(text);
        debugDrawer.addShape(r.hidden);
        shapes.arrow = arrow;
        shapes.text = text;
        delete r.hidden;
    }
    if (event.id === "debug:remove") {
        shapes.arrow.remove();
        shapes.text.color = { red: 0, green: 1, blue: 0, alpha: 0.5 };
    }
    if (event.id === "debug:stats") {
        const tab = diagnosticsManager.createTab("Perf");
        r.tab = tab.tabName + "|" + diagnosticsManager.tabs.length + "|" + diagnosticsManager.containsTab(tab);
        r.duplicate = errorOf(() => diagnosticsManager.createTab("Perf"));
        r.reason = (() => { try { diagnosticsManager.createTab(""); } catch (e) {
            return (e instanceof DiagnosticsError) + "|" + e.reason + "|" + e.message; } })();
        r.emptyView = errorOf(() => diagnosticsManager.createView(""));
        const view = diagnosticsManager.createView("ticks", { chartType: DiagnosticsChartDisplayType.LineChart });
        r.viewAdded = diagnosticsManager.containsView(view);
        tab.addView(view);
        r.viewAdded += "|" + diagnosticsManager.containsView(view) + "|" + tab.views.length;
        view.pushStats([{ name: "a", values: [1, "b"] }]);
        r.pushTwice = errorOf(() => view.pushStats([]));
        r.reasons = DiagnosticsErrorReason.InvalidData;
        const runtime = collectRuntimeStats();
        r.runtime = (runtime.objectCount > 0) + "|" + (runtime.memoryAllocatedLimit > 0) + "|"
            + (runtime.functionCount > 0) + "|" + typeof runtime.fastArrayElementCount;
        world.getAllPlayers();
        const plugins = collectPluginStats().plugins;
        r.plugins = plugins.length + "|" + plugins[0].name + "|" + plugins[0].packId + "|"
            + plugins[0].scriptModuleUUID + "|" + (plugins[0].handleCounts.Player >= 1);
        disableWatchdogTimingWarnings(true);
        r.watchdog = errorOf(() => disableWatchdogTimingWarnings("yes"));
        world.setDynamicProperty("debug", JSON.stringify(r));
    }
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner, const std::string &key) {
        const auto it = owner.getWorldDynamicProperties().find(key);
        require(it != owner.getWorldDynamicProperties().end(), "no result from the " + key + " pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the " + key + " pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        std::string actual = value->mString;
        if (value->mType == json::Value::Type::Boolean)
            actual = value->mBoolean ? "true" : "false";
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    // the PrimitiveShapes packets one tick sends
    std::vector<PrimitiveShapesPacket> sentShapes(ServerNetworkHandler &owner, ScriptEngine &engine, int64_t tick) {
        std::vector<PrimitiveShapesPacket> packets;
        owner.getNetworkHandler().setOutboundFilter([&packets, &owner](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            if (Packet::peekId(header, sender, client) == MinecraftPacketIds::PrimitiveShapes) {
                ReadOnlyBinaryStream body(data);
                PrimitiveShapesPacket packet;
                packet.readHeader(body);
                packet.read(body, owner.getCodecContext());
                packets.push_back(packet);
            }
            return false;
        });
        engine.tick(tick);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        return packets;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "debug",
                  "{\"module_name\": \"@minecraft/server\", \"version\": \"2.12.0-beta\"},"
                  " {\"module_name\": \"@minecraft/debug-utilities\", \"version\": \"1.0.0-beta\"},"
                  " {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  DEBUG);

        ServerNetworkHandler owner("Script debug", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the debug pack did not start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 3001).first->second;
        engine.startup();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        player.setName("DebugPlayer");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);

        engine.queueScriptEvent("debug:add", "");
        std::vector<PrimitiveShapesPacket> added = sentShapes(owner, engine, 1);
        require(added.size() == 1 && added[0].mShapes.size() == 4, "the visible shapes did not go out in one packet");
        const DebugShapeData &box = added[0].mShapes[0];
        require(box.mHasType && box.mType == DebugShapeType::Box && box.mColor == 0xFFFF0000u && box.mDimension == 0
                        && box.mBoxBounds.x == 2.0f && box.mPosition.z == 3.0f && box.mScale == 1.0f
                        && box.mTotalTimeLeft > 0.0f && box.mMaximumRenderDistance == -1.0f,
                "the box snapshot is wrong");
        const DebugShapeData &arrow = added[0].mShapes[1];
        require(arrow.mType == DebugShapeType::Arrow && arrow.mArrowHeadLength == 1.0f && arrow.mArrowHeadRadius == 0.5f
                        && arrow.mArrowHeadSegments == 4 && arrow.mArrowEndPosition.y == 5.0f,
                "the arrow defaults are wrong");
        const DebugShapeData &cylinder = added[0].mShapes[2];
        require(cylinder.mType == DebugShapeType::Cylinder && cylinder.mRadii2f.x == 1.5f && cylinder.mHeight == 1.0f
                        && cylinder.mSegments == 20 && cylinder.mDimension == 3,
                "the cylinder snapshot is wrong");
        const DebugShapeData &text = added[0].mShapes[3];
        require(text.mType == DebugShapeType::Text && text.mText == "plain" && text.mShowBackface
                        && !text.mShowTextBackface,
                "the text snapshot is wrong");
        require(sentShapes(owner, engine, 2).empty(), "unchanged shapes went out again");

        // 0.12 seconds: the box expires on its third tick
        engine.queueScriptEvent("debug:remove", "");
        std::vector<PrimitiveShapesPacket> changed = sentShapes(owner, engine, 3);
        require(changed.size() == 1 && changed[0].mShapes.size() == 3, "the removals and the update did not go out");
        size_t removals = 0;
        bool recolored = false;
        for (const DebugShapeData &shape: changed[0].mShapes) {
            removals += shape.mHasType ? 0 : 1;
            recolored = recolored || (shape.mType == DebugShapeType::Text && shape.mColor == 0x7F00FF00u);
        }
        require(removals == 2 && recolored, "the expired box, the removed arrow or the recolored text is missing");

        engine.queueScriptEvent("debug:stats", "");
        engine.tick(4);
        const std::unique_ptr<json::Value> debug = result(owner, "debug");
        expect(*debug, "early",
               "ReferenceError:Native constructor for [DebugBox::constructor] cannot be used in early "
               "execution.");
        expect(*debug, "box", "1|minecraft:overworld|true|0.12|{\"x\":1,\"y\":2,\"z\":3}|true");
        expect(*debug, "arrow", "1|0.5|4|{\"x\":0,\"y\":5,\"z\":0}|true");
        expect(*debug, "segments", "ArgumentOutOfBoundsError:");
        expect(*debug, "segmentsError", "3|128|200");
        expect(*debug, "distance", "ArgumentOutOfBoundsError:");
        expect(*debug, "text", "{\"rawtext\":[{\"text\":\"hi\"}]}|true|false");
        expect(*debug, "constructor", "ReferenceError:No constructor for native class 'DebugShape'.");
        expect(*debug, "drawer", "ReferenceError:No constructor for native class 'DebugDrawer'.");
        expect(*debug, "badLocation", "TypeError:Native type conversion failed.");
        expect(*debug, "tab", "Perf|1|true");
        expect(*debug, "duplicate", "DiagnosticsError:Tab with the name 'Perf' already exists!");
        expect(*debug, "reason", "true|InvalidName|tabName must be non-empty");
        expect(*debug, "emptyView", "DiagnosticsError:statName must be non-empty");
        expect(*debug, "viewAdded", "false|true|1");
        expect(*debug, "pushTwice", "DiagnosticsError:Stats have already been pushed this frame!");
        expect(*debug, "reasons", "InvalidData");
        expect(*debug, "runtime", "true|true|true|number");
        expect(*debug, "plugins",
               "1|debug|00000000-0000-4000-8000-400000000001|00000000-0000-4000-9000-400000000001|true");
        expect(*debug, "watchdog", "TypeError:Native type conversion failed.");
        require(engine.getApis()[0]->timingWarningsDisabled(), "disableWatchdogTimingWarnings did not stick");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-debug-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        // the server writes a restrictive default module list when there is none: these packs use every module
        writeFile(directory / "config" / "default" / "permissions.json", R"({"allowed_modules": ["*"]})");
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script debug regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
