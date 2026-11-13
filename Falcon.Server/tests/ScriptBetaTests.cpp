#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Server/Commands/TellCommand.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/GameRules.h"
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
                          + "\", \"uuid\": \"00000000-0000-4000-8000-400000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-400000000001\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const BETA = R"JS(
import { world, system, BlockBoundingBoxUtils, BlockVolume, ListBlockVolume, PoiBlockOccupancyFilter,
    WatchdogTerminateReason } from "@minecraft/server";

const r = {};
const log = (line) => { r.events = (r.events || "") + line + ";"; };
const save = () => world.setDynamicProperty("beta", JSON.stringify(r));
const round = (v) => Math.round(v * 100) / 100;

world.beforeEvents.playerPlaceBlock.subscribe((e) => {
    log("place:" + e.permutationToPlace.type.id + ":" + e.face + ":" + e.faceLocation.x + ":" + e.block.x + ","
        + e.block.y + "," + e.block.z + ":" + e.player.name + ":" + e.dimension.id);
    e.cancel = e.permutationToPlace.type.id === "minecraft:dirt";
});
world.beforeEvents.playerItemAttackEntity.subscribe((e) => {
    const aim = e.aimDirection === undefined ? "none"
        : round(e.aimDirection.x) + "," + round(e.aimDirection.y) + "," + round(e.aimDirection.z);
    log("attack:" + e.target.typeId + ":" + (e.itemStack === undefined ? "empty" : e.itemStack.typeId) + ":" + aim);
    e.cancel = true;
});
world.afterEvents.playerUseNameTag.subscribe((e) => {
    log("named:" + e.entityNamed.typeId + ":" + e.newName + ":" + String(e.previousName) + ":" + e.player.name);
});
world.beforeEvents.chatSend.subscribe((e) => {
    log("beforeChat:" + e.message + ":" + (e.targets === undefined ? "all" : e.targets.map((p) => p.name).join(",")));
    e.cancel = e.message === "stop";
});
world.afterEvents.chatSend.subscribe((e) => {
    log("chat:" + e.message + ":" + (e.targets === undefined ? "all" : e.targets.map((p) => p.name).join(",")));
});

system.afterEvents.scriptEventReceive.subscribe((event) => {
    const overworld = world.getDimension("overworld");
    if (event.id === "beta:weather") {
        r.weather = (r.weather === undefined ? "" : r.weather + ",") + overworld.getWeather();
        r.netherWeather = world.getDimension("nether").getWeather();
    } else if (event.id === "beta:thunder") {
        overworld.setWeather("Thunder");
    } else if (event.id === "beta:boxes") {
        const U = BlockBoundingBoxUtils;
        const v = (p) => p.x + "," + p.y + "," + p.z;
        const b = (box) => box === undefined ? "undefined" : v(box.min) + ">" + v(box.max);
        const box = U.createValid({ x: 3.9, y: -1.5, z: 0 }, { x: -2.2, y: 4, z: 1 });
        const other = { min: { x: 1, y: 1, z: 1 }, max: { x: 6, y: 6, z: 6 } };
        r.boxes = [b(box), b(U.dilate(box, { x: 1, y: 1.9, z: -3 })), U.equals(box, box), U.equals(box, other),
            b(U.expand(box, other)), v(U.getCenter(other)), b(U.getIntersection(box, other)),
            b(U.getIntersection(box, { min: { x: 9, y: 9, z: 9 }, max: { x: 10, y: 10, z: 10 } })),
            v(U.getSpan(box)), U.intersects(box, other), U.isInside(box, { x: -1.9, y: 4.9, z: 1 }),
            U.isInside(box, { x: 4, y: 0, z: 0 }), U.isValid(box),
            U.isValid({ min: { x: 1, y: 0, z: 0 }, max: { x: 0, y: 0, z: 0 } }),
            b(U.translate(box, { x: 1.5, y: -2.5, z: 0 }))].join("|");
        r.volumeBox = b(new BlockVolume({ x: 2, y: 5, z: 1 }, { x: -1, y: 3, z: 4 }).getBoundingBox());
        try { new ListBlockVolume([]).getBoundingBox(); r.emptyBox = "ok"; } catch (e) { r.emptyBox = e.name + ":" + e.message; }
        r.boxCtor = (() => { try { new U(); return "ok"; } catch (e) { return e.name; } })();
        r.enums = PoiBlockOccupancyFilter.Any + ":" + WatchdogTerminateReason.Hang;
        const stoneBlock = overworld.getBlock({ x: 1, y: 99, z: 1 });
        stoneBlock.setType("minecraft:stone");
        const rgba = (c) => [c.red, c.green, c.blue, c.alpha].map((v) => Math.round(v * 255)).join(",");
        r.mapColor = rgba(stoneBlock.getMapColor()) + "|" + rgba(overworld.getBlock({ x: 1, y: 110, z: 1 }).getMapColor());
        world.broadcastClientMessage("test:id", "hello");
        const carried = world.getLootTableManager().getLootTable("test/carry").pools[0].entries[0].functions;
        r.carry = carried.map((f) => f.constructor.name + ":" + f.dynamicProperties).join(",");
        const banners = world.getLootTableManager().getLootTable("test/banner").pools[0].entries[0].functions;
        r.banner = banners.map((f) => f.type + ":" + f.baseColor + ":" +
            f.patterns.map((p) => p.constructor.name + "=" + p.pattern + "/" + p.color).join(";")).join(",");
    } else if (event.id === "beta:settings") {
        const rules = world.gameRules;
        r.waypoints = rules.playerWaypoints;
        rules.playerWaypoints = "Off";
        r.waypointsOff = rules.playerWaypoints;
        try { rules.playerWaypoints = "None"; r.waypointsBad = "ok"; } catch (e) { r.waypointsBad = e.name; }
        r.cheats = world.allowCheats;
        world.allowCheats = false;
        r.cheatsOff = world.allowCheats;
        try { world.allowCheats = 1; r.cheatsBad = "ok"; } catch (e) { r.cheatsBad = e.name; }
        const player = world.getPlayers().find((p) => p.name === "Alex");
        r.splitScreen = String(player.getSplitScreenSlot());
        const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
        const camera = player.camera;
        r.ease = [fail(() => camera.setCameraWithEase("minecraft:free", { easeTime: 1, easeType: "Linear" })),
            fail(() => camera.setCameraWithEase("minecraft:first_person", { easeTime: 1 })),
            fail(() => camera.setCameraWithEase("minecraft:free", { easeTime: -1 })),
            fail(() => camera.setCameraWithEase("minecraft:nope", {})),
            fail(() => camera.setCameraWithEase("minecraft:first_person", {})),
            fail(() => camera.setCameraWithEase("minecraft:free"))].join("|");
        const breath = overworld.getEntities({ type: "minecraft:cow" })[0].getComponent("breathable");
        const fullAir = breath.airSupply;
        breath.airSupply = 4;
        r.air = [fullAir, breath.airSupply, fail(() => { breath.airSupply = -2; }),
            fail(() => { breath.airSupply = 16; }), fail(() => { breath.airSupply = "x"; })].join("|");
        overworld.stopSound("random.click");
        overworld.stopAllSounds();
        r.stopped = "ok";
    }
    save();
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("beta");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the beta pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the beta pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    ServerPlayer &addPlayer(ServerNetworkHandler &owner, Level &level, const char *name, const Vector3f &position) {
        static unsigned long long serial = 3000;
        ++serial;
        const NetworkIdentifier id(std::to_string(serial), serial);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, serial).first->second;
        player.setName(name);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(position);
        return player;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "beta", BETA);
        writeFile(packs / "beta" / "loot_tables" / "test" / "carry.json",
                  "{\"pools\": [{\"rolls\": 1, \"entries\": [{\"type\": \"item\", \"name\": \"minecraft:chest\","
                  " \"functions\": [{\"function\": \"carry_over_block_entity_data\", \"dynamic_properties\": true},"
                  " {\"function\": \"carry_over_block_entity_data\", \"dynamic_properties\": 1}]}]}]}");
        writeFile(packs / "beta" / "loot_tables" / "test" / "banner.json",
                  "{\"pools\": [{\"rolls\": 1, \"entries\": [{\"type\": \"item\", \"name\": \"minecraft:banner\","
                  " \"functions\": [{\"function\": \"set_banner_details\", \"type\": 0, \"base_color\": \"LIGHT_BLUE\","
                  " \"patterns\": [{\"pattern\": \"creeper\", \"color\": \"red\"}, {\"pattern\": \"bogus\"},"
                  " {\"pattern\": \"cross\", \"color\": 20}, {\"pattern\": \"skull\", \"color\": 4},"
                  " {\"pattern\": \"flow\"}, {\"pattern\": \"globe\"}, {\"pattern\": \"base\"},"
                  " {\"pattern\": \"border\"}]},"
                  " {\"function\": \"set_banner_details\", \"type\": 1, \"patterns\": [{\"pattern\": \"creeper\"}]},"
                  " {\"function\": \"set_banner_details\", \"base_color\": 7,"
                  " \"patterns\": {\"pattern\": \"bricks\", \"color\": \"black\"}}]}]}]}");

        ServerNetworkHandler owner("Script beta", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the beta pack did not start");

        engine.startup();
        EntityDefinitions::initialize();
        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        ServerPlayer &player = addPlayer(owner, level, "Alex", Vector3f(0.5f, 100.0f, 0.5f));
        addPlayer(owner, level, "Sam", Vector3f(3.5f, 100.0f, 0.5f));
        engine.tick(0);

        const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
        const BlockState dirt = VanillaBlocks::fromIdentifier("minecraft:dirt")->toBlockState();
        require(!engine.beforePlayerPlaceBlock(player, level, Vector3i(1, 100, 2), 1, Vector3f(0.25f, 1.0f, 0.5f),
                                               stone),
                "an uncancelled placement was cancelled");
        require(engine.beforePlayerPlaceBlock(player, level, Vector3i(1, 100, 2), 1, Vector3f(0.25f, 1.0f, 0.5f), dirt),
                "playerPlaceBlock did not cancel the placement");

        ServerActor *cow = owner.spawnActor(level, "minecraft:cow", Vector3f(0.5f, 100.0f, 2.5f));
        MobActor *mob = dynamic_cast<MobActor *>(cow);
        require(mob != nullptr, "no fixture cow");
        mob->startDefinition(owner);
        const float health = cow->getHealth();
        // eye at 101.62: the hit is straight ahead along z
        const Vector3f hit(0.5f, 101.62f, 2.5f);
        require(!player.attackActor(owner, cow->getRuntimeId(), &hit) && cow->getHealth() == health,
                "a cancelled attack still hurt the cow");

        ItemStack tag = owner.createItemStack("minecraft:name_tag", 1);
        Tag display = Tag::ofCompound();
        display.putString("Name", "Bessie");
        tag.mTag = Tag::ofCompound();
        tag.mTag.put("display", display);
        player.getInventory().setItemInHand(tag);
        require(cow->onInteract(owner, player) && cow->getNameTag() == "Bessie", "the name tag did not name the cow");
        display.putString("Name", "Daisy");
        tag.mTag.put("display", display);
        player.getInventory().setItemInHand(tag);
        require(cow->onInteract(owner, player), "the second name tag did not rename the cow");

        TellCommand tell(owner);
        PlayerCommandOrigin origin(owner, player, CommandOriginData());
        require(tell.execute(origin, {"Sam", "hi"}), "tell did not deliver");
        require(!tell.execute(origin, {"Sam", "stop"}), "a cancelled tell was delivered");

        engine.emitScriptEvent("beta:weather", "");
        engine.emitScriptEvent("beta:thunder", "");
        engine.emitScriptEvent("beta:weather", "");
        for (int tick = 0; tick < 21; ++tick)
            owner.tickWeather();
        engine.emitScriptEvent("beta:weather", "");
        for (int tick = 0; tick < 79; ++tick)
            owner.tickWeather();
        engine.emitScriptEvent("beta:weather", "");

        require(owner.getProperties().getAllowCheats(), "the fixture starts without cheats");
        engine.emitScriptEvent("beta:settings", "");
        engine.emitScriptEvent("beta:boxes", "");
        require(!owner.getProperties().getAllowCheats(), "allowCheats did not turn cheats off");
        const GameRules::Rule *waypoints = level.getGameRules().find("playerwaypoints");
        require(waypoints != nullptr && waypoints->mIntValue == 0, "playerWaypoints did not reach the game rule");

        const std::unique_ptr<json::Value> r = result(owner);
        expect(*r, "events",
               "place:minecraft:stone:Up:0.25:1,100,2:Alex:minecraft:overworld;"
               "place:minecraft:dirt:Up:0.25:1,100,2:Alex:minecraft:overworld;"
               "attack:minecraft:cow:empty:0,0,1;"
               "named:minecraft:cow:Bessie:undefined:Alex;named:minecraft:cow:Daisy:Bessie:Alex;"
               "beforeChat:hi:Sam;chat:hi:Sam;beforeChat:stop:Sam;");
        expect(*r, "weather", "Clear,Clear,Rain,Thunder");
        expect(*r, "netherWeather", "Clear");
        expect(*r, "waypoints", "Everyone");
        expect(*r, "waypointsOff", "Off");
        expect(*r, "waypointsBad", "TypeError");
        expect(*r, "cheats", "true");
        expect(*r, "cheatsOff", "false");
        expect(*r, "cheatsBad", "TypeError");
        expect(*r, "stopped", "ok");
        // a cow holds 15 s of air, suffocate_time 0
        expect(*r, "air",
               "15|4|PropertyOutOfBoundsError:Unsupported or out of bounds value set on Property: airSupply, Value: -2, "
               "Property bounds: [0, 15]|PropertyOutOfBoundsError:Unsupported or out of bounds value set on Property: "
               "airSupply, Value: 16, Property bounds: [0, 15]|TypeError:Native type conversion failed.");
        expect(*r, "splitScreen", "undefined");
        expect(*r, "boxes",
               "-2,-1,0>3,4,1|-3,-2,-2>4,5,3|true|false|-2,-1,0>6,6,6|4,4,4|1,1,1>3,4,1|undefined|6,6,2|true|true|false|"
               "true|false|-1,-3,0>4,2,1");
        expect(*r, "volumeBox", "-1,3,1>2,5,4");
        expect(*r, "emptyBox", "Error:Cannot get bounding box of empty block volume.");
        expect(*r, "boxCtor", "ReferenceError");
        expect(*r, "enums", "Any:Hang");
        expect(*r, "mapColor", "112,112,112,255|0,0,0,0");
        expect(*r, "carry", "CarryOverBlockEntityDataFunction:true,CarryOverBlockEntityDataFunction:false");
        expect(*r, "banner",
               "0:lightBlue:BannerPattern=creeper/red;BannerPattern=cross/white;BannerPattern=skull/blue;"
               "BannerPattern=flow/white;BannerPattern=globe/white;BannerPattern=base/white,1:white:,0:silver:"
               "BannerPattern=bricks/black");
        expect(*r, "ease",
               "ok|Error:The camera preset selected is not an allowed ease target.|Error:Easing time cannot be less "
               "than 0.|Error:Invalid camera preset!|ok|TypeError:Native type conversion failed.");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-beta-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script beta regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
