#include "Core/Json/Json.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CameraAimAssistPacket.h"
#include "Protocol/Packets/CameraAimAssistPresetsPacket.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
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
#include <utility>
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

    void writePack(const std::filesystem::path &root, const std::string &script) {
        writeFile(
                root / "aim" / "manifest.json",
                "{\"format_version\": 2, \"header\": {\"name\": \"aim\", \"uuid\": "
                "\"00000000-0000-4000-8000-600000000001\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, 0]},"
                " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                " \"uuid\": \"00000000-0000-4000-9000-600000000001\", \"entry\": \"scripts/main.js\","
                " \"version\": [1, 0, 0]}],"
                " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"2.11.0\"}]}\n");
        writeFile(root / "aim" / "scripts" / "main.js", script);
    }

    const char *const SCRIPT = R"JS(
import { world, system, AimAssistRegistry, AimAssistCategorySettings, AimAssistPresetSettings } from "@minecraft/server";

const r = {};
const error = (f) => {
    try { f(); return "ok"; } catch (e) {
        return [e.name, e.message, e.type, e.index, e.reason].filter((v) => v !== undefined).join("|");
    }
};
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "aim:run")
        return;
    const registry = world.getAimAssist();
    r.statics = AimAssistRegistry.DefaultCategoryId + "," + AimAssistRegistry.DefaultPresetId;
    r.presets = registry.getPresets().map((p) => p.identifier).join(",");
    r.categories = registry.getCategories().map((c) => c.identifier).join(",");
    const vanilla = registry.getPreset(AimAssistRegistry.DefaultPresetId);
    r.vanilla = [vanilla.defaultItemSettings, vanilla.handSettings, vanilla.getExcludedBlockTargets().join(";"),
        vanilla.getExcludedEntityTargets().join(";"), vanilla.getExcludedBlockTagTargets().length,
        JSON.stringify(vanilla.getItemSettings()), vanilla.getLiquidTargetingItems().length].join(",");
    const bucket = registry.getCategory("minecraft:bucket");
    r.bucket = [bucket.defaultEntityPriority, bucket.defaultBlockPriority,
        bucket.getBlockPriorities()["minecraft:cauldron"]].join(",");
    const fallback = registry.getCategory("minecraft:default");
    r.unsetDefaults = fallback.defaultEntityPriority + "," + fallback.defaultBlockPriority;
    r.missing = String(registry.getPreset("test:none")) + "," + String(registry.getCategory("test:none"));

    const category = new AimAssistCategorySettings("test:cat");
    r.categoryDefaults = category.defaultEntityPriority + "," + category.defaultBlockPriority;
    category.setBlockPriorities({ "minecraft:stone": 5.9 });
    category.defaultEntityPriority = 7;
    const added = registry.addCategory(category);
    r.category = [added.identifier, added.defaultEntityPriority, added.defaultBlockPriority,
        JSON.stringify(added.getBlockPriorities()), JSON.stringify(added.getEntityPriorities())].join(",");
    r.emptyCategory = error(() => registry.addCategory(new AimAssistCategorySettings("")));
    r.noNamespace = error(() => registry.addCategory(new AimAssistCategorySettings("cat")));
    r.twoNamespaces = error(() => registry.addCategory(new AimAssistCategorySettings("a:b:c")));
    r.notSettings = error(() => registry.addCategory({ identifier: "test:x" }));

    const preset = new AimAssistPresetSettings("test:preset");
    r.unsetList = String(preset.getExcludedBlockTargets()) + "," + String(preset.defaultItemSettings);
    preset.setItemSettings({ "minecraft:stick": "test:cat" });
    preset.setExcludedEntityTargets(["minecraft:cow"]);
    const made = registry.addPreset(preset);
    r.preset = [made.identifier, made.defaultItemSettings, made.handSettings, made.getExcludedBlockTargets().length,
        made.getExcludedEntityTargets().join(";"), JSON.stringify(made.getItemSettings())].join(",");
    const unknown = new AimAssistPresetSettings("test:broken");
    unknown.handSettings = "test:none";
    r.unknownCategory = error(() => registry.addPreset(unknown));
    unknown.handSettings = "none";
    r.categoryNamespace = error(() => registry.addPreset(unknown));
    r.emptyPreset = error(() => registry.addPreset(new AimAssistPresetSettings("")));

    const aim = world.getAllPlayers()[0].getAimAssist();
    r.before = String(aim.settings);
    aim.set({ presetId: "test:preset", targetMode: "Distance", distance: 4 });
    r.settings = JSON.stringify(aim.settings);
    r.angle = error(() => aim.set({ presetId: "test:preset", viewAngle: { x: 5, y: 20 } }));
    r.distance = error(() => aim.set({ presetId: "test:preset", distance: 16.5 }));
    r.unknownPreset = error(() => aim.set({ presetId: "test:missing" }));
    r.emptyId = error(() => aim.set({ presetId: "" }));
    r.kept = aim.settings.presetId;
    world.setDynamicProperty("aim", JSON.stringify(r));
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "aim:clear")
        return;
    const aim = world.getAllPlayers()[0].getAimAssist();
    aim.set(undefined);
    world.setDynamicProperty("cleared", String(aim.settings));
});
)JS";

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        require(value->string() == wanted,
                std::string(key) + ": expected \"" + wanted + "\", got \"" + value->string() + "\"");
    }

    template <typename T>
    std::vector<T> decoded(ServerNetworkHandler &owner,
                           const std::vector<std::pair<MinecraftPacketIds, std::string>> &sent) {
        std::vector<T> packets;
        for (const auto &entry: sent) {
            if (entry.first != T::ID)
                continue;
            ReadOnlyBinaryStream stream(entry.second);
            T packet;
            packet.readHeader(stream);
            packet.read(stream, owner.getCodecContext());
            packets.push_back(std::move(packet));
        }
        return packets;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, SCRIPT);

        ServerNetworkHandler owner("Script aim assist", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the aim assist pack did not start");
        engine.startup();

        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        player.setName("Alex");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        engine.tick(0);

        require(owner.isTransportReady(), "the test server has no network handler to observe");
        std::vector<std::pair<MinecraftPacketIds, std::string>> sent;
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });

        // a joining player gets the vanilla registry, with the game's sync operation
        engine.sendAimAssist(player);
        std::vector<CameraAimAssistPresetsPacket> joined = decoded<CameraAimAssistPresetsPacket>(owner, sent);
        require(joined.size() == 1 && joined[0].mOperation == CameraAimAssistOperation::AddToExisting
                        && joined[0].mPresets.size() == 1 && joined[0].mCategoryDefinitions.size() == 3,
                "the join sync does not carry the vanilla aim assist registry");
        const CameraAimAssistPresetDefinition &vanilla = joined[0].mPresets[0];
        require(vanilla.mIdentifier == "minecraft:aim_assist_default" && vanilla.mHasDefaultItemSettings
                        && vanilla.mDefaultItemSettings == "minecraft:default" && vanilla.mHasHandSettings
                        && vanilla.mHandSettings == "minecraft:empty_hand" && vanilla.mItemSettings.size() == 1
                        && vanilla.mBlockExclusionList.size() == 1 && vanilla.mLiquidTargetingList.size() == 19,
                "the vanilla preset does not follow the pack's aim_assist_preset.json");
        const CameraAimAssistCategory &defaults = joined[0].mCategoryDefinitions[2];
        require(defaults.mName == "minecraft:default" && !defaults.mHasActorDefaultPriorities
                        && !defaults.mHasBlockDefaultPriorities && defaults.mBlockPriorities.size() == 5,
                "the vanilla default category does not follow the pack's categories.json");
        sent.clear();

        engine.emitScriptEvent("aim:run", "");
        const auto it = owner.getWorldDynamicProperties().find("aim");
        require(it != owner.getWorldDynamicProperties().end(), "the aim assist pack wrote nothing");
        const std::unique_ptr<json::Value> r = json::parse(it->second.mString);
        require(r != nullptr && r->isObject(), "the aim assist pack wrote no JSON");
        expect(*r, "statics", "minecraft:default,minecraft:aim_assist_default");
        expect(*r, "presets", "minecraft:aim_assist_default,test:preset");
        expect(*r, "categories", "minecraft:bucket,minecraft:empty_hand,minecraft:default,test:cat");
        expect(*r, "vanilla",
               "minecraft:default,minecraft:empty_hand,minecraft:bedrock,minecraft:arrow,0,"
               "{\"minecraft:bucket\":\"minecraft:bucket\"},19");
        expect(*r, "bucket", "30,30,60");
        expect(*r, "unsetDefaults", "1,1");
        expect(*r, "missing", "undefined,undefined");
        expect(*r, "categoryDefaults", "1,1");
        expect(*r, "category", "test:cat,7,1,{\"minecraft:stone\":5},{}");
        expect(*r, "emptyCategory", "InvalidArgumentError|Invalid provided value to argument 'categoryId'.|Empty|0");
        expect(*r, "noNamespace",
               "NamespaceNameError|There was an error with name 'cat': string must be prefixed with a namespace "
               "(eg. namespace:value).|NoNamespace");
        expect(*r, "twoNamespaces",
               "NamespaceNameError|There was an error with name 'a:b:c': string cannot have multiple "
               "namespaces.|DisallowedNamespace");
        expect(*r, "notSettings", "TypeError|Native type conversion failed.");
        expect(*r, "unsetList", "undefined,undefined");
        expect(*r, "preset", "test:preset,default,hand,0,minecraft:cow,{\"minecraft:stick\":\"test:cat\"}");
        expect(*r, "unknownCategory", "InvalidArgumentError|Unknown category Id 'test:none'.|Unknown|0");
        expect(*r, "categoryNamespace",
               "NamespaceNameError|There was an error with name 'none': string must be prefixed with a namespace "
               "(eg. namespace:value).|NoNamespace");
        expect(*r, "emptyPreset", "InvalidArgumentError|Invalid provided value to argument 'presetId'.|Empty|0");
        expect(*r, "before", "undefined");
        expect(*r, "settings",
               "{\"presetId\":\"test:preset\",\"targetMode\":\"Distance\",\"viewAngle\":{\"x\":50,\"y\":50},"
               "\"distance\":4}");
        expect(*r, "angle",
               "ArgumentOutOfBoundsError|Unsupported or out of bounds value passed to function argument [0]: "
               "viewAngle.x, Value: 5, Argument bounds: [10, 90]|0");
        expect(*r, "distance",
               "ArgumentOutOfBoundsError|Unsupported or out of bounds value passed to function argument [0]: "
               "distance, Value: 16.50, Argument bounds: [1, 16]|0");
        expect(*r, "unknownPreset",
               "InvalidArgumentError|Invalid provided value to argument 'test:missing'.|Unknown|0");
        expect(*r, "emptyId", "InvalidArgumentError|Invalid provided value to argument 'presetId'.|Empty|0");
        expect(*r, "kept", "test:preset");

        // each added entry reaches the players on its own
        const std::vector<CameraAimAssistPresetsPacket> added = decoded<CameraAimAssistPresetsPacket>(owner, sent);
        require(added.size() == 2 && added[0].mCategoryDefinitions.size() == 1 && added[0].mPresets.empty()
                        && added[0].mCategoryDefinitions[0].mName == "test:cat"
                        && added[0].mCategoryDefinitions[0].mHasActorDefaultPriorities
                        && added[0].mCategoryDefinitions[0].mActorDefaultPriorities == 7
                        && added[0].mCategoryDefinitions[0].mHasBlockDefaultPriorities
                        && added[0].mCategoryDefinitions[0].mBlockDefaultPriorities == 1,
                "addCategory did not sync the new category");
        require(added[1].mPresets.size() == 1 && added[1].mPresets[0].mIdentifier == "test:preset"
                        && added[1].mPresets[0].mHasDefaultItemSettings
                        && added[1].mPresets[0].mDefaultItemSettings == "default"
                        && added[1].mPresets[0].mHasHandSettings && added[1].mPresets[0].mHandSettings == "hand",
                "addPreset did not sync the new preset with the game's fallbacks");
        const std::vector<CameraAimAssistPacket> set = decoded<CameraAimAssistPacket>(owner, sent);
        require(set.size() == 1 && set[0].mAction == AimAssistAction::Set && set[0].mPresetId == "test:preset"
                        && set[0].mTargetMode == CameraAimAssistPacket::TargetMode::Distance
                        && set[0].mViewAngle.x == 50.0f && set[0].mViewAngle.y == 50.0f && set[0].mDistance == 4.0f,
                "set did not send the player's aim assist");
        sent.clear();

        engine.emitScriptEvent("aim:clear", "");
        const auto cleared = owner.getWorldDynamicProperties().find("cleared");
        require(cleared != owner.getWorldDynamicProperties().end() && cleared->second.mString == "undefined",
                "set(undefined) kept the settings");
        const std::vector<CameraAimAssistPacket> clear = decoded<CameraAimAssistPacket>(owner, sent);
        require(clear.size() == 1 && clear[0].mAction == AimAssistAction::Clear && clear[0].mPresetId.empty()
                        && clear[0].mViewAngle.x == 30.0f && clear[0].mViewAngle.y == 45.0f
                        && clear[0].mDistance == 5.7f,
                "set(undefined) did not send the game's clear packet");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-aim-assist-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script aim assist regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
