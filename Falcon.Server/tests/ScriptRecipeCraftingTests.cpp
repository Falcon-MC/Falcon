#include "Core/Json/Json.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ClientboundStonecutterSetRecipePacket.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
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
        writeFile(root / "crafting" / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"crafting\", \"uuid\": "
                  "\"00000000-0000-4000-8000-600000000001\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, "
                  "0]}, \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                  " \"uuid\": \"00000000-0000-4000-9000-600000000001\", \"entry\": \"scripts/main.js\","
                  " \"version\": [1, 0, 0]}],"
                  " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / "crafting" / "scripts" / "main.js", script);
    }

    const char *const SCRIPT = R"JS(
import { world, system, ItemStack, BlockRecipeCraftingComponent, RecipeCraftingContext } from "@minecraft/server";

const r = {};
const save = () => world.setDynamicProperty("crafting", JSON.stringify(r));
const fail = (f) => {
    try { f(); return "ok"; } catch (e) {
        return [e.name, e.message, e.recipeId, e.index, e.minValue, e.maxValue, e.value]
            .filter((v) => v !== undefined).join("|");
    }
};
const at = (x, z) => world.getDimension("overworld").getBlock({ x, y: 100, z });
const item = (stack) => stack === undefined ? "undefined" : stack.typeId + "x" + stack.amount;
let kept;

system.afterEvents.scriptEventReceive.subscribe((event) => {
    const player = world.getAllPlayers()[0];
    const crafting = at(5, 5).getComponent("recipe_crafting");
    if (event.id === "crafting:closed") {
        r.component = (crafting instanceof BlockRecipeCraftingComponent) + ":" + crafting.typeId + ":"
            + BlockRecipeCraftingComponent.componentId;
        r.stoneCrafting = String(at(6, 6).getComponent("recipe_crafting"));
        r.listed = at(5, 5).getComponents().some((c) => c.typeId === "minecraft:recipe_crafting");
        r.closedPlayers = crafting.players.length;
        r.closedContext = String(crafting.getCraftingContext(player));
    } else if (event.id === "crafting:open") {
        const context = crafting.getCraftingContext(player);
        kept = context;
        r.context = (context instanceof RecipeCraftingContext) + ":" + context.isValid + ":" + context.inputSlotCount;
        r.players = crafting.players.map((p) => p.name).join(",");
        r.input = item(context.getInputItem(0));
        const recipes = context.validRecipes;
        r.recipes = recipes.length + ":" + recipes[0] + ":" + recipes[1];
        r.output = item(context.getOutputItem());
        context.setSelectedRecipe("minecraft:stonecutter_andesite_stairs");
        r.selected = item(context.getOutputItem());
        r.wrongRecipe = fail(() => context.setSelectedRecipe("minecraft:stonecutter_cobblestone_from_stone"));
        r.slot = fail(() => context.getInputItem(1));
        r.setSlot = fail(() => context.setInputItem(2, new ItemStack("minecraft:stone", 1)));
        context.setInputItem(0, new ItemStack("minecraft:stone", 3));
        r.stone = item(context.getInputItem(0)) + ":" + context.validRecipes[0] + ":" + item(context.getOutputItem());
        context.setInputItem(0);
        r.cleared = item(context.getInputItem(0)) + ":" + context.validRecipes.length + ":"
            + item(context.getOutputItem());
        r.emptyRecipe = fail(() => context.setSelectedRecipe("minecraft:stonecutter_andesite_slab"));
        context.setInputItem(0, new ItemStack("minecraft:andesite", 5));
    } else if (event.id === "crafting:gone") {
        r.gone = kept.isValid + ":" + fail(() => kept.validRecipes) + ":" + fail(() => kept.getOutputItem());
        r.gonePlayers = crafting.players.length;
    }
    save();
});
)JS";

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    std::vector<ClientboundStonecutterSetRecipePacket>
    selections(ServerNetworkHandler &owner, const std::vector<std::pair<MinecraftPacketIds, std::string>> &sent) {
        std::vector<ClientboundStonecutterSetRecipePacket> packets;
        for (const auto &entry: sent) {
            if (entry.first != ClientboundStonecutterSetRecipePacket::ID)
                continue;
            ReadOnlyBinaryStream stream(entry.second);
            ClientboundStonecutterSetRecipePacket packet;
            packet.readHeader(stream);
            packet.read(stream, owner.getCodecContext());
            packets.push_back(packet);
        }
        return packets;
    }

    ItemStack makeItem(ServerNetworkHandler &owner, const char *identifier, int count) {
        ItemStack item = ItemStack::air();
        item.mDefinition = owner.getItemDefinitions().getDefinition(identifier);
        item.mBlockDefinition = owner.getBlockDefinitions().getDefinition(identifier);
        item.mCount = count;
        return item;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, SCRIPT);

        ServerNetworkHandler owner("Script recipe crafting", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the recipe crafting pack did not start");
        engine.startup();

        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i cutterAt(5, 100, 5);
        level.setBlock(cutterAt, BlockState("minecraft:stonecutter_block"), false);
        level.setBlock(Vector3i(6, 100, 6), BlockState("minecraft:stone"), false);

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        player.setName("Alex");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(5.5f, 101.0f, 5.5f));
        InventoryManager &inventory = player.getInventoryManager();
        inventory.attach(&player, &owner);
        engine.tick(0);

        require(owner.isTransportReady(), "the test server has no network handler to observe");
        std::vector<std::pair<MinecraftPacketIds, std::string>> sent;
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });

        engine.emitScriptEvent("crafting:closed", "");
        owner.tick(false);

        require(inventory.onClientOpenStonecutter(cutterAt), "the stonecutter window did not open");
        player.getInventory().setStonecutterInput(makeItem(owner, "minecraft:andesite", 5));
        // the client's own pick: out of range is dropped like the game, a valid one is kept
        inventory.onClientSelectStonecutterRecipe(inventory.getStonecutterWindowId(), 40);
        require(inventory.getStonecutterSelectedRecipe() == 0, "an out of range client pick was kept");
        inventory.onClientSelectStonecutterRecipe(inventory.getStonecutterWindowId() + 1, 2);
        require(inventory.getStonecutterSelectedRecipe() == 0, "a pick for another window was kept");
        inventory.onClientSelectStonecutterRecipe(inventory.getStonecutterWindowId(), 2);
        require(inventory.getStonecutterSelectedRecipe() == 2, "a valid client pick was dropped");
        inventory.onClientSelectStonecutterRecipe(inventory.getStonecutterWindowId(), 0);
        sent.clear();

        engine.emitScriptEvent("crafting:open", "");
        owner.tick(false);

        const std::vector<ClientboundStonecutterSetRecipePacket> picks = selections(owner, sent);
        require(picks.size() == 1, "the script pick did not reach the client alone");
        require(picks[0].mRecipeIndex == 1, "the client was told the wrong recipe index");
        require(picks[0].mPlayerUniqueId == player.getUniqueId()
                        && picks[0].mContainerId == (uint8_t) inventory.getStonecutterWindowId(),
                "the recipe pick names the wrong player or window");
        require(player.getInventory().getStonecutterInput().mCount == 5, "setInputItem did not reach the inventory");

        inventory.onClientRemoveWindow(inventory.getStonecutterWindowId());
        require(player.getInventory().getStonecutterInput().isAir(), "closing the stonecutter kept its input");
        engine.emitScriptEvent("crafting:gone", "");
        owner.tick(false);

        const auto it = owner.getWorldDynamicProperties().find("crafting");
        require(it != owner.getWorldDynamicProperties().end(), "the recipe crafting pack wrote nothing");
        const std::unique_ptr<json::Value> r = json::parse(it->second.mString);
        require(r != nullptr && r->isObject(), "the recipe crafting pack wrote no JSON");
        expect(*r, "component", "true:minecraft:recipe_crafting:minecraft:recipe_crafting");
        expect(*r, "stoneCrafting", "undefined");
        expect(*r, "listed", "true");
        expect(*r, "closedPlayers", "0");
        expect(*r, "closedContext", "undefined");
        expect(*r, "context", "true:true:1");
        expect(*r, "players", "Alex");
        expect(*r, "input", "minecraft:andesitex5");
        expect(*r, "recipes", "6:minecraft:stonecutter_andesite_slab:minecraft:stonecutter_andesite_stairs");
        expect(*r, "output", "minecraft:andesite_slabx2");
        expect(*r, "selected", "minecraft:andesite_stairsx1");
        expect(*r, "wrongRecipe",
               "InvalidRecipeError|Invalid recipe Id minecraft:stonecutter_cobblestone_from_stone|"
               "minecraft:stonecutter_cobblestone_from_stone");
        expect(*r, "slot",
               "ArgumentOutOfBoundsError|Unsupported or out of bounds value passed to function argument [0]: slot, "
               "Value: 1, Argument bounds: [0, 1]|0|0|1|1");
        expect(*r, "setSlot",
               "ArgumentOutOfBoundsError|Unsupported or out of bounds value passed to function argument [0]: slot, "
               "Value: 2, Argument bounds: [0, 1]|0|0|1|2");
        expect(*r, "stone",
               "minecraft:stonex3:minecraft:stonecutter_cobblestone_from_stone:minecraft:cobblestone_slabx2");
        expect(*r, "cleared", "undefined:0:undefined");
        expect(*r, "emptyRecipe",
               "InvalidRecipeError|Invalid recipe Id minecraft:stonecutter_andesite_slab|"
               "minecraft:stonecutter_andesite_slab");
        expect(*r, "gone",
               "false:EngineError|RecipeCraftingContext is invalid.:EngineError|RecipeCraftingContext is invalid.");
        expect(*r, "gonePlayers", "0");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-recipe-crafting-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script recipe crafting regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
