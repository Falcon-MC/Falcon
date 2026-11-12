#include "Core/Event/EventBus.h"
#include "Core/Json/Json.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/VanillaBlocks.h"
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

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &version,
                   const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-5000000000" + (version[0] == '2' ? "02" : "01")
                          + "\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-5000000000"
                          + (version[0] == '2' ? "02" : "01")
                          + "\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \""
                          + version + "\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    // each subscription logs its tag; the filters decide which tags appear
    const char *const STABLE2 = R"JS(
import { world, system } from "@minecraft/server";

const r = { events: "" };
const log = (tag) => { r.events += tag + ";"; world.setDynamicProperty("filters", JSON.stringify(r)); };
const on = (signal, tag, options) => signal.subscribe(() => log(tag), options);
const after = world.afterEvents;
const before = world.beforeEvents;

system.afterEvents.scriptEventReceive.subscribe((e) => {
    if (e.id !== "filters:arm")
        return;
    const player = world.getPlayers()[0];
    on(after.entityDie, "dieCow", { entityTypes: ["cow"] });
    on(after.entityDie, "diePlayer", { entities: [player] });
    on(after.entityDie, "dieNone", { entityTypes: ["minecraft:pig"] });
    on(after.entityHurt, "hurtMagic", { allowedDamageCauses: ["magic"] });
    on(after.entityHurt, "hurtFall", { allowedDamageCauses: ["fall"] });
    on(after.entityHurt, "hurtPlayer", { entityFilter: { type: "minecraft:player" } });
    on(before.entityHurt, "beforeHurtCow", { entityTypes: ["minecraft:cow"], allowedDamageCauses: ["magic"] });
    on(before.entityHurt, "beforeHurtFall", { allowedDamageCauses: ["fall"] });
    on(after.entityHitEntity, "hitByPlayer", { entityTypes: ["minecraft:player"] });
    on(after.entityHitEntity, "hitByCow", { entityTypes: ["minecraft:cow"] });
    on(after.entityHitBlock, "hitBlock", { entities: [player] });
    on(after.entityStartSneaking, "sneakPlayer", { entityFilter: { type: "minecraft:player" } });
    on(after.entityStartSneaking, "sneakCow", { entityFilter: { type: "minecraft:cow" } });
    on(after.entityTamed, "tamedByPlayer", { tamingEntityFilter: { type: "minecraft:player" } });
    on(after.entityTamed, "tamedByCow", { tamingEntityFilter: { type: "minecraft:cow" } });
    on(after.dataDrivenEntityTrigger, "triggerGrow", { eventTypes: ["minecraft:ageable_grow_up"] });
    on(after.dataDrivenEntityTrigger, "triggerOther", { eventTypes: ["minecraft:become_angry"] });
    on(after.entityRemove, "removeCow", { entityTypes: ["minecraft:cow"] });
    on(after.entityRemove, "removePig", { entityTypes: ["minecraft:pig"] });
    on(after.entityItemDrop, "dropDiamond", { itemFilter: { includeTypes: ["diamond"] } });
    on(after.entityItemDrop, "dropStone", { itemFilter: { includeTypes: ["minecraft:stone"] } });
    on(after.entityHeal, "healRegen", { allowedHealCauses: ["Regeneration"] });
    on(after.entityHeal, "healTotem", { allowedHealCauses: ["TotemOfUndying"] });
    on(after.playerButtonInput, "jumpPressed", { buttons: ["Jump"], state: "Pressed" });
    on(after.playerButtonInput, "sneakButton", { buttons: ["Sneak"] });
    on(after.playerButtonInput, "released", { state: "Released" });
    on(after.playerSwingStart, "swingEmpty", { heldItemOption: "NoItem", swingSource: "Attack" });
    on(after.playerSwingStart, "swingHolding", { heldItemOption: "AnyItem" });
    on(after.playerSwingStart, "swingBuild", { swingSource: "Build" });
    on(after.playerHotbarSelectedSlotChange, "slotFour", { allowedSlots: [4] });
    on(after.playerHotbarSelectedSlotChange, "slotOne", { allowedSlots: [1] });
    on(after.playerBreakBlock, "breakStone", { blockTypes: ["stone"] });
    on(after.playerBreakBlock, "breakDirt", { blockTypes: ["minecraft:dirt"] });
    on(after.playerPlaceBlock, "placeStone", { permutations: [BlockPermutation.resolve("minecraft:stone")] });
    on(after.playerPlaceBlock, "placeDirt", { permutations: [BlockPermutation.resolve("minecraft:dirt")] });
    on(after.effectAdd, "effectPlayer", { entityTypes: ["player"] });
    on(after.effectAdd, "effectCow", { entityTypes: ["cow"] });
    log("armed");
});
)JS";

    // 1.x hands EntityEventOptions to entityHurt
    const char *const STABLE1 = R"JS(
import { world, system } from "@minecraft/server";

let events = "";
system.afterEvents.scriptEventReceive.subscribe((e) => {
    if (e.id !== "filters:arm")
        return;
    world.afterEvents.entityHurt.subscribe(() => { events += "hurtCow;"; world.setDynamicProperty("legacy", events); },
        { entityTypes: ["minecraft:cow"] });
    world.afterEvents.entityHurt.subscribe(() => { events += "hurtPig;"; world.setDynamicProperty("legacy", events); },
        { entityTypes: ["minecraft:pig"] });
});
)JS";

    std::string result(ServerNetworkHandler &owner, const char *key) {
        const auto it = owner.getWorldDynamicProperties().find(key);
        require(it != owner.getWorldDynamicProperties().end(), std::string("no result under ") + key);
        return it->second.mString;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        std::string stable2 = STABLE2;
        stable2.replace(stable2.find("{ world, system }"), 17, "{ world, system, BlockPermutation }");
        writePack(packs, "filters", "2.11.0", stable2);
        writePack(packs, "legacy", "1.19.0", STABLE1);

        ServerNetworkHandler owner("Script filters", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 2, "the filter packs did not start");

        engine.startup();
        EntityDefinitions::initialize();
        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        player.setName("Alex");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        ServerActor *cow = owner.spawnActor(level, "minecraft:cow", Vector3f(0.5f, 100.0f, 2.5f));
        MobActor *mob = dynamic_cast<MobActor *>(cow);
        require(mob != nullptr, "no fixture cow");
        mob->startDefinition(owner);
        engine.tick(0);
        engine.emitScriptEvent("filters:arm", "");

        const ActorDamageSource magic = ActorDamageSource::environment("death.attack.magic", "Alex");
        engine.onEntityDie(*cow, magic);
        engine.onEntityDie(player, magic);
        engine.onEntityHurt(*cow, 1.0f, magic);
        engine.onEntityHurt(player, 1.0f, magic);
        float damage = 1.0f;
        engine.beforeEntityHurt(*cow, damage, magic);
        engine.beforeEntityHurt(player, damage, magic);
        engine.onEntityHitEntity(player, *cow);
        engine.onEntityHitBlock(player, Vector3i(0, 99, 0), 1);
        engine.onEntityHitBlock(*cow, Vector3i(0, 99, 0), 1);
        engine.onEntitySneak(player, true);
        engine.onEntityTamed(*cow, player);
        engine.onDataDrivenEntityTrigger(*mob, "minecraft:ageable_grow_up", {}, {});
        ItemActor *diamond = ItemActorHandler::dropItem(owner, level, Vector3f(0.5f, 100.0f, 1.5f),
                                                        owner.createItemStack("minecraft:diamond", 1),
                                                        Vector3f(0.0f, 0.0f, 0.0f), 0);
        require(diamond != nullptr, "no fixture item entity");
        engine.onEntityItemDrop(player, diamond);
        player.setHealth(10.0f);
        player.heal(2.0f, HealCause::Regeneration);
        engine.onPlayerButtonInput(player, "Jump", true);
        engine.onPlayerButtonInput(player, "Jump", false);
        player.getInventory().setItemInHand(ItemStack::air());
        engine.onPlayerSwingStart(player, "Attack");
        PlayerHotbarSelectedSlotChangeAfterEvent slot(player, 4, 0, "");
        owner.getEventBus().after().mPlayerHotbarSelectedSlotChange.emit(slot);
        PlayerBreakBlockAfterEvent broken(player, Vector3i(0, 99, 0), "minecraft:stone");
        owner.getEventBus().after().mPlayerBreakBlock.emit(broken);
        level.setBlock(Vector3i(1, 100, 1), VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState(), false);
        PlayerPlaceBlockAfterEvent placed(player, Vector3i(1, 100, 1), "minecraft:stone");
        owner.getEventBus().after().mPlayerPlaceBlock.emit(placed);
        MobEffectInstance speed;
        speed.mId = MobEffectId::Speed;
        speed.mDuration = 40;
        engine.onEffectAdd(player, speed);
        engine.onEntityRemove(*cow);

        const std::unique_ptr<json::Value> parsed = json::parse(result(owner, "filters"));
        require(parsed != nullptr && parsed->get("events") != nullptr, "the filter pack wrote no JSON");
        const std::string events = parsed->get("events")->string();
        const std::string wanted = "armed;dieCow;diePlayer;hurtMagic;hurtMagic;hurtPlayer;beforeHurtCow;hitByPlayer;"
                                   "hitBlock;sneakPlayer;tamedByPlayer;triggerGrow;dropDiamond;healRegen;jumpPressed;"
                                   "released;swingEmpty;slotFour;breakStone;placeStone;effectPlayer;removeCow;";
        require(events == wanted, "filtered events: expected \"" + wanted + "\", got \"" + events + "\"");
        require(result(owner, "legacy") == "hurtCow;", "1.x entityHurt ignored its entityTypes");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-filters-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script event filter regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
