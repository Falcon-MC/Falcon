#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Event/EventBus.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Scripting/Binding/ScriptCustomCommand.h"
#include "Scripting/Binding/ScriptNatives.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/ScriptCommandOrigin.h"
#include "Server/Commands/ExecuteCommandOrigin.h"
#include "Server/Localization.h"
#include "Server/Commands/ScriptEventCommand.h"
#include "Protocol/Packets/AvailableCommandsPacket.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/ServerPaths.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/EnchantmentHelper.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/StringToItemParser.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/BlockSolidity.h"
#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Scripting/WorldClockSystem.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/LoginHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/ModalFormRequestPacket.h"
#include "Protocol/Packets/ModalFormResponsePacket.h"
#include "Protocol/Packets/ClientboundDataDrivenUIShowScreenPacket.h"
#include "Protocol/Packets/ClientboundDataStorePacket.h"
#include "Protocol/Packets/GraphicsOverrideParameterPacket.h"
#include "Protocol/Packets/ServerboundDataDrivenScreenClosedPacket.h"
#include "Protocol/Packets/ServerboundDataStorePacket.h"
#include "Protocol/Packets/UpdateClientOptionsPacket.h"
#include "Protocol/Packets/DimensionDataPacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/Packets/SyncWorldClocksPacket.h"
#include "World/Level/WorldManager.h"
#include "World/Actor/MobEffect.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Vehicle/ChestVehicleActor.h"
#include "World/Inventory/InventoryManager.h"
#include "Protocol/Packets/ContainerOpenPacket.h"
#include "Network/Handler/ItemActorHandler.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"

#include <chrono>
#include <cmath>
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

    // a script pack: dependencies is the manifest's JSON array body
    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &dependencies,
                   const std::string &script, const std::string &extra = std::string()) {
        static int serial = 0;
        ++serial;
        const std::string header = "00000000-0000-4000-8000-" + std::to_string(100000000000 + serial);
        const std::string module = "00000000-0000-4000-9000-" + std::to_string(100000000000 + serial);
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2,\n"
                  " \"header\": {\"name\": \""
                          + name + "\", \"uuid\": \"" + header
                          + "\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},\n"
                            " // comments are allowed in manifests\n"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": \""
                          + module
                          + "\","
                            " \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],\n"
                            " \"dependencies\": ["
                          + dependencies + "]" + extra + "}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const PRELUDE = R"JS(
const r = {};
const capture = (f) => {
    try {
        f();
        return "ok";
    } catch (e) {
        return e.name + ": " + e.message;
    }
};
globalThis["marker_" + Math.random()] = true;
r.alone = Object.keys(globalThis).filter((k) => k.startsWith("marker_")).length === 1;
r.internals = Object.getOwnPropertyNames(globalThis).filter((k) => k.startsWith("__falcon")).length;
)JS";

    const char *const STABLE2 = R"JS(
import { world, system, Entity, Player, ItemStack, Dimension, World, GameMode, InputPermissionCategory,
         PlayerInventoryType, EntityInventoryComponent, EntityComponentTypes, InvalidEntityError,
         ScreenDisplay, TickingAreaManager, Difficulty, TimeOfDay, MoonPhase, WeatherType, ScriptEventSource,
         GameRule, MemoryTier } from "@minecraft/server";
import * as server from "@minecraft/server";
import { EngineError, InvalidArgumentError, InvalidArgumentErrorType } from "@minecraft/common";

r.early = capture(() => world.getAllPlayers());
r.earlyRun = capture(() => system.run(() => {}));
r.entityConstructor = capture(() => new Entity());
r.playerExtendsEntity = Object.getPrototypeOf(Player.prototype) === Entity.prototype
    && Object.getPrototypeOf(Player) === Entity;
r.worldIsWorld = world instanceof World;
r.gameMode = GameMode.Survival + "," + GameMode.Spectator;
r.sneak = InputPermissionCategory.Sneak + "," + InputPermissionCategory.MoveRight;
r.inventoryType = PlayerInventoryType.Inventory + "," + PlayerInventoryType.Hotbar;
r.componentId = EntityInventoryComponent.componentId + "," + EntityComponentTypes.Inventory;
r.kick = "kick" in Player.prototype;
r.hideAllForms = "hideAllForms" in ScreenDisplay.prototype;
r.ticking = ("getAllTickingAreas" in TickingAreaManager.prototype) && !("getTickingAreas" in TickingAreaManager.prototype);
r.worldInitialize = "worldInitialize" in world.beforeEvents;
r.itemUseOn = "itemUseOn" in world.beforeEvents;
r.chatSend = ("chatSend" in world.afterEvents) || ("chatSend" in world.beforeEvents);
r.handle = Object.getOwnPropertyNames(Entity.prototype).filter((k) => k.startsWith("_")).length;
r.legacyExport = "WorldInitializeBeforeEvent" in server;
r.engineError = capture(() => new EngineError());
const argument = new InvalidArgumentError("f", "string", InvalidArgumentErrorType.Empty, 1);
r.invalidArgument = argument instanceof Error && argument.index === 1 && argument.type === "Empty";
r.enums = [Difficulty.Hard, TimeOfDay.Noon, MoonPhase.NewMoon, WeatherType.Thunder, ScriptEventSource.NPCDialogue,
           GameRule.KeepInventory, MemoryTier.SuperHigh].join(",");
r.signals = [world.afterEvents, world.beforeEvents, system.afterEvents, system.beforeEvents]
    .map((holder) => Object.keys(holder).filter((key) => typeof holder[key].subscribe === "function"
        && typeof holder[key].unsubscribe === "function").length).join(",");
r.v1Signals = ["gameRuleChange", "weatherChange"].every((key) => key in world.afterEvents)
    && "weatherChange" in world.beforeEvents && "scriptEventReceive" in system.afterEvents;

world.afterEvents.gameRuleChange.subscribe((event) => {
    r.gameRule = event.rule + "=" + event.value + "," + typeof event.value;
});
let weatherCalls = 0;
world.beforeEvents.weatherChange.subscribe((event) => {
    weatherCalls++;
    r.weatherBefore = (r.weatherBefore || "") + event.previousWeather + ">" + event.newWeather + ";";
    if (weatherCalls === 1) {
        event.cancel = true;
        return;
    }
    event.newWeather = WeatherType.Thunder;
    event.duration = 1234;
});
world.afterEvents.weatherChange.subscribe((event) => {
    r.weatherAfter = (r.weatherAfter || "") + [event.dimension, event.previousWeather, event.newWeather].join("|") + ";";
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    r.scriptEvents = (r.scriptEvents || "") + [event.id, event.message, event.sourceType,
        event.sourceEntity instanceof Player, event.initiator === undefined,
        event.sourceBlock === undefined].join("|") + ";";
});
const filtered = system.afterEvents.scriptEventReceive.subscribe((event) => {
    r.filtered = (r.filtered || "") + event.id + ";";
}, { namespaces: ["other"] });
const removed = (event) => { r.removed = event.id; };
system.afterEvents.scriptEventReceive.subscribe(removed, { namespaces: ["test"] });
system.afterEvents.scriptEventReceive.unsubscribe(removed);

let registry;
system.beforeEvents.startup.subscribe((event) => {
    r.startupEarly = capture(() => world.getAllPlayers());
    r.register = capture(() => event.blockComponentRegistry.registerCustomComponent("test:comp", {}));
    registry = event.blockComponentRegistry;
});

let saved;
world.beforeEvents.effectAdd.subscribe((event) => {
    if (saved !== undefined || event.entity.typeId !== "minecraft:player")
        return;
    saved = event.entity;
    r.restrictedWrite = capture(() => event.entity.addTag("x"));
    r.restrictedRead = capture(() => event.entity.getTags());
    r.restrictedRun = capture(() => system.run(() => {
        r.player = saved instanceof Player && saved instanceof Entity;
        r.same = world.getDimension("overworld") === world.getDimension("minecraft:overworld");
    }));
    r.amplifier = "amplifier" in event;
});

world.afterEvents.worldLoad.subscribe(() => {
    r.lateRegister = capture(() => registry.registerCustomComponent("test:late", {}));
    r.prototypeBound = capture(() => Entity.prototype.addTag("x"));
    r.normal = capture(() => world.getAllPlayers());
    r.item = new ItemStack("minecraft:stone", 2) instanceof ItemStack;
    r.dimension = world.getDimension("overworld") instanceof Dimension;
    world.setDynamicProperty("stable2", JSON.stringify(r));
});

system.runTimeout(() => {
    r.invalid = capture(() => saved.location);
    r.invalidClass = (() => {
        try {
            saved.location;
        } catch (e) {
            return e instanceof InvalidEntityError;
        }
        return false;
    })();
    r.isValid = saved.isValid;
    world.setDynamicProperty("stable2", JSON.stringify(r));
}, 6);

import("../../outside.js").then(() => { r.escape = "loaded"; }, () => { r.escape = "blocked"; });
)JS";

    const char *const WORLD = R"JS(
import { world, system, GameRules, TimeOfDay, MoonPhase, Difficulty, MemoryTier, NamespaceNameError } from "@minecraft/server";

r.rulesConstructor = capture(() => new GameRules());
r.earlyRules = capture(() => world.gameRules);
r.editor = system.isEditorWorld;
r.earlyTier = capture(() => system.serverSystemInfo.memoryTier);
r.earlySettings = capture(() => world.getPackSettings());
r.settings = JSON.stringify(world.getPackSettings());
system.waitTicks(2).then(() => { r.waited = true; });
r.waitZero = capture(() => system.waitTicks(0));

world.beforeEvents.effectAdd.subscribe(() => {
    r.restrictedRuleRead = capture(() => world.gameRules.pvp);
    r.restrictedRuleWrite = capture(() => { world.gameRules.pvp = false; });
    r.restrictedTime = capture(() => world.setTimeOfDay(TimeOfDay.Noon));
    r.restrictedDifficulty = capture(() => world.setDifficulty(Difficulty.Hard));
    r.restrictedSpawn = capture(() => world.setDefaultSpawnLocation({ x: 0, y: 64, z: 0 }));
});

system.runTimeout(() => {
    r.stableRules = world.gameRules === world.gameRules;
    // the tier follows the machine's memory: any of the five values
    r.memoryTier = [MemoryTier.SuperLow, MemoryTier.Low, MemoryTier.Mid, MemoryTier.High, MemoryTier.SuperHigh]
        .includes(system.serverSystemInfo.memoryTier);
    r.pvp = world.gameRules.pvp;
    world.gameRules.pvp = false;
    world.gameRules.randomTickSpeed = 3;
    r.rules = world.gameRules.pvp + "," + world.gameRules.randomTickSpeed;
    r.ruleType = capture(() => { world.gameRules.keepInventory = 1; });
    world.setAbsoluteTime(24000 * 9 + 100);
    world.setTimeOfDay(TimeOfDay.Night);
    r.time = world.getAbsoluteTime() + "," + world.getTimeOfDay() + "," + world.getDay();
    r.moon = world.getMoonPhase() === MoonPhase.WaningGibbous;
    r.timeRange = capture(() => world.setTimeOfDay(24001));
    world.setDifficulty(Difficulty.Hard);
    r.difficulty = world.getDifficulty();
    world.setDefaultSpawnLocation({ x: 10, y: 70, z: -5 });
    const spawn = world.getDefaultSpawnLocation();
    r.spawn = spawn.x + "," + spawn.y + "," + spawn.z;
    r.spawnBounds = capture(() => world.setDefaultSpawnLocation({ x: 0, y: 5000, z: 0 }));
    r.players = world.getPlayers().length + "," + world.getPlayers({ name: "nobody" }).length;
    r.seed = typeof world.seed;
    r.hardcore = world.isHardcore;
    world.setDynamicProperties({ a: 1, b: "xy" });
    r.bytes = world.getDynamicPropertyTotalByteCount() > 0;
    r.noNamespace = capture(() => system.sendScriptEvent("noname", "m"));
    r.minecraftNamespace = (() => {
        try {
            system.sendScriptEvent("minecraft:x", "m");
        } catch (e) {
            return (e instanceof NamespaceNameError) + "," + e.reason;
        }
        return "ok";
    })();
    r.sendEvent = capture(() => system.sendScriptEvent("test:event", "m"));
}, 5);
system.runTimeout(() => world.setDynamicProperty("world", JSON.stringify(r)), 7);
)JS";

    const char *const ENTITIES = R"JS(
import { world, system, Entity, Player, Effect, EffectTypes, ItemStack, BlockPermutation, EntityTypes, EntityComponentTypes, EntityHealthComponent,
         EntityTypeFamilyComponent, EntityDamageCause, EntityInitializationCause, EntityHealCause, InputButton,
         ButtonState, InputMode, EntitySwingSource, CommandPermissionLevel, PlayerPermissionLevel,
         TickingAreaManager, EntityComponent, Seat, FeedItem, Trigger, CatmullRomSpline,
         LinearSpline } from "@minecraft/server";
import { Scoreboard, ScoreboardObjective, ScoreboardIdentity, ScoreboardScoreInfo, DisplaySlotId, ObjectiveSortOrder,
         ScoreboardIdentityType } from "@minecraft/server";

const log = (line) => { r.events = (r.events || "") + line + ";"; };
world.afterEvents.entitySpawn.subscribe((e) => log("spawn:" + e.entity.typeId + ":" + e.cause));
world.beforeEvents.entityHurt.subscribe((e) => {
    if (e.hurtEntity.typeId === "minecraft:cow" && e.damage === 5)
        e.damage = 3;
});
world.afterEvents.entityHurt.subscribe((e) => log("hurt:" + e.hurtEntity.typeId + ":" + e.damage + ":"
    + e.damageSource.cause + ":" + (e.damageSource.damagingEntity instanceof Player)));
world.afterEvents.entityDie.subscribe((e) => log("die:" + e.deadEntity.typeId + ":" + e.damageSource.cause));
world.beforeEvents.effectAdd.subscribe((e) => {
    if (e.effectType === "speed")
        e.duration = 40;
    if (e.effectType === "poison")
        e.cancel = true;
});
world.afterEvents.effectAdd.subscribe((e) => log("effect:" + e.effect.typeId + ":" + e.effect.duration
    + ":" + (e.effect instanceof Effect)));
world.afterEvents.entityHealthChanged.subscribe((e) => {
    if (e.entity.typeId === "minecraft:player")
        log("health:" + e.oldValue + ">" + e.newValue);
});
world.beforeEvents.entityHeal.subscribe((e) => { e.healing = 2; });
world.afterEvents.entityHeal.subscribe((e) => log("heal:" + e.healing + ":" + e.healSource.cause));
world.afterEvents.dataDrivenEntityTrigger.subscribe((e) => {
    if (e.eventId === "minecraft:ageable_grow_up")
        log("trigger:" + e.eventId + ":" + JSON.stringify(e.getModifiers()));
});
world.beforeEvents.entityRemove.subscribe((e) => log("beforeRemove:" + e.removedEntity.typeId));
world.afterEvents.entityRemove.subscribe((e) => log("remove:" + e.typeId));
world.beforeEvents.playerGameModeChange.subscribe((e) => {
    if (e.toGameMode === "Adventure")
        e.toGameMode = "Creative";
});
world.afterEvents.playerGameModeChange.subscribe((e) => log("gm:" + e.fromGameMode + ">" + e.toGameMode));
let emotePlayer;
world.afterEvents.playerEmote.subscribe((e) => {
    log("emote:" + e.personaPieceId);
    emotePlayer = e.player;
    r.idBefore = e.player.id;
});
world.afterEvents.playerButtonInput.subscribe((e) => {
    log("button:" + e.button + ":" + e.newButtonState);
    r.idAfter = (e.player === emotePlayer) + ":" + e.player.id;
});
world.afterEvents.playerInputModeChange.subscribe((e) => log("mode:" + e.previousInputModeUsed + ">"
    + e.newInputModeUsed));
world.afterEvents.playerSwingStart.subscribe((e) => log("swing:" + e.swingSource));
world.afterEvents.entityStartSneaking.subscribe((e) => log("sneak:" + e.entity.typeId));
world.afterEvents.entityStopSneaking.subscribe((e) => log("unsneak:" + e.entity.typeId));
world.beforeEvents.entityTamed.subscribe((e) => log("beforeTame:" + e.entity.typeId));
world.afterEvents.entityTamed.subscribe((e) => log("tamed:" + e.entity.typeId + ":" + (e.tamingEntity instanceof Player)));
world.afterEvents.playerDimensionChange.subscribe((e) => log("dimension:" + e.fromDimension.id + ">"
    + e.toDimension.id));
world.beforeEvents.playerLeave.subscribe((e) => log("leaving:" + (e.player instanceof Player)));
world.afterEvents.playerInteractWithEntity.subscribe((e) => log("interact:" + e.target.typeId));

r.enums = [EntityDamageCause.void, EntityDamageCause.entityAttack, EntityInitializationCause.Born,
           EntityHealCause.SelfHeal, InputButton.Jump, ButtonState.Pressed, InputMode.Touch, EntitySwingSource.Attack,
           CommandPermissionLevel.Admin, PlayerPermissionLevel.Operator].join(",");
r.componentIds = [EntityHealthComponent.componentId, EntityTypeFamilyComponent.componentId,
                  EntityComponentTypes.Movement].join(",");
r.componentChain = Object.getPrototypeOf(EntityHealthComponent.prototype) !== null
    && EntityHealthComponent.prototype instanceof EntityComponent;
r.typesConstructor = (() => { try { new EffectTypes(); return "ok"; } catch (e) { return e.name; } })();

system.runTimeout(() => {
  try {
    const overworld = world.getDimension("overworld");
    const cow = overworld.getEntities({ type: "minecraft:cow" })[0];
    const player = world.getPlayers()[0];
    r.cowFound = cow instanceof Entity;
    r.families = cow.getComponent("type_family").getTypeFamilies().join(",");
    r.familyQuery = overworld.getEntities({ families: ["mob"], type: "minecraft:cow" }).length + ","
        + overworld.getEntities({ excludeFamilies: ["cow"], type: "minecraft:cow" }).length;
    const health = cow.getComponent(EntityComponentTypes.Health);
    r.health = (health instanceof EntityHealthComponent) + ":" + health.effectiveMax + ":" + health.currentValue
        + ":" + (health.entity === cow);
    health.setCurrentValue(4);
    r.healthSet = health.currentValue + ":" + health.setCurrentValue(1000) + ":" + health.setCurrentValue(NaN) + ":"
        + health.currentValue;
    health.resetToMaxValue();
    r.componentsCount = cow.getComponents().length > 5;
    r.movement = cow.hasComponent("minecraft:movement") + ":" + (cow.getComponent("movement").currentValue > 0);
    r.missing = cow.getComponent("minecraft:tameable") === undefined && !cow.hasComponent("minecraft:is_tamed");
    r.navigation = cow.getComponent("minecraft:navigation.walk").avoidWater + ":" + cow.getComponent("minecraft:navigation.walk").canWalk;
    r.localization = cow.localizationKey + "," + EntityTypes.get("cow").localizationKey + ","
        + EntityTypes.get("minecraft:cow").id + "," + (EntityTypes.get("nope:nope") === undefined)
        + "," + (EntityTypes.getAll().length > 50);
    r.effectTypes = EffectTypes.get("minecraft:speed").getName() + "," + (EffectTypes.getAll().length > 20)
        + "," + (EffectTypes.get("nope") === undefined);
    const added = cow.addEffect("speed", 200, { amplifier: 1 });
    r.addEffect = (added instanceof Effect) + ":" + added.amplifier + ":" + added.duration + ":" + added.isValid;
    r.effectName = added.displayName + "|" + cow.addEffect("haste", 200).displayName;
    cow.removeEffect("haste");
    r.cancelledEffect = cow.addEffect(EffectTypes.get("poison"), 100) === undefined;
    r.effects = cow.getEffects().map((effect) => effect.typeId).join(",");
    cow.removeEffect("speed");
    r.removedEffect = added.isValid;
    const box = cow.getAABB();
    r.aabb = (box.extent.y > 0) + ":" + (box.center.y > cow.location.y);
    r.states = [cow.isOnGround, cow.isInWater, cow.isFalling, cow.isClimbing, cow.isSwimming, cow.isSleeping].join(",");
    cow.isSneaking = true;
    r.sneaking = cow.isSneaking;
    cow.isSneaking = false;
    r.hurt = cow.applyDamage(5, { cause: EntityDamageCause.fall, damagingEntity: player });
    r.projectileOption = (() => { try { cow.applyDamage(1, { damagingProjectile: {} }); return "ok"; }
                                  catch (e) { return e.name; } })();
    cow.triggerEvent("minecraft:ageable_grow_up");
    cow.setRotation({ x: 10, y: 20 });
    r.rotation = cow.getRotation().x + "," + cow.getRotation().y;
    cow.lookAt({ x: cow.location.x, y: cow.location.y, z: cow.location.z + 10 });
    r.lookAt = Math.round(cow.getRotation().y);
    const viewer = overworld.getEntities({ type: "minecraft:wolf" })[0];
    viewer.lookAt(player.location);
    const seen = viewer.getEntitiesFromViewDirection({ maxDistance: 20, ignoreBlockCollision: true });
    r.entityRay = seen.length + ":" + (seen.length > 0 && seen[0].entity === player) + ":"
        + (seen.length > 0 ? seen[0].distance.toFixed(2) : "-")
        + ":" + viewer.getEntitiesFromViewDirection({ maxDistance: 0, ignoreBlockCollision: true }).length
        + ":" + viewer.getEntitiesFromViewDirection({ maxDistance: 20, excludeTypes: ["minecraft:player"],
                                                    ignoreBlockCollision: true }).length;
    r.blockRay = cow.getBlockFromViewDirection() === undefined;
    player.setRotation({ x: 0, y: 0 });
    const fence = player.getBlockFromViewDirection();
    r.shapeRay = fence.block.typeId + ":" + fence.face + ":" + fence.faceLocation.z.toFixed(3);
    const stone = player.getBlockFromViewDirection({ excludeTypes: ["oak_fence"] });
    const permutation = player.getBlockFromViewDirection({ includePermutations: [BlockPermutation.resolve("minecraft:stone")] });
    r.filterRay = stone.block.location.z + ":" + permutation.block.location.z + ":"
        + (player.getBlockFromViewDirection({ maxDistance: 4 }) === undefined) + ":"
        + (player.getBlockFromViewDirection({ excludeTags: ["stone"], excludeTypes: ["oak_fence"] }) === undefined);
    const horse = overworld.getEntities({ type: "minecraft:horse" })[0];
    // the spawn event rolls a foal at random: make it an adult, wild horse
    if (!horse.hasComponent("minecraft:tamemount"))
        horse.triggerEvent("minecraft:spawn_adult");
    const mount = horse.getComponent("minecraft:tamemount");
    mount.tame(false);
    r.tameMount = mount.isTamed + ":" + mount.isTamedToPlayer + ":" + (mount.tamedToPlayerId === undefined);
    const seat = horse.getComponent("rideable").getSeats()[0];
    r.seat = [seat instanceof Seat, seat.lockRiderRotation, seat.maxRiderCount, seat.position.y.toFixed(2),
              seat.seatRotation].join(":");
    const feed = horse.getComponent("healable").getFeedItems();
    r.feed = [feed.length, feed[0] instanceof FeedItem, feed[0].item, feed[0].healAmount, feed[0].resultItem,
              feed[0].getEffects().length].join(":");
    // the fixture's cow grew up above: its born event makes it a calf again
    const calf = overworld.getEntities({ type: "minecraft:cow" })[0];
    calf.triggerEvent("minecraft:entity_born");
    const ageable = calf.getComponent("ageable");
    r.ageable = ageable.getFeedItems().map((item) => item.item + "@" + (item.growth === Math.fround(0.1))).join("|")
        + ":" + ageable.growUp.eventName + ":" + (ageable.growUp instanceof Trigger);
    r.emoting = player.isEmoting;
    r.betaAbsent = !("chatDisplayName" in Player.prototype) && !("eatItem" in Player.prototype);
    player.commandPermissionLevel = CommandPermissionLevel.GameDirectors;
    r.op = player.commandPermissionLevel + ":" + player.playerPermissionLevel;
    const promise = world.tickingAreaManager.createTickingArea("test:pending", { dimension: overworld,
        from: { x: 4000, y: 0, z: 4000 }, to: { x: 4000, y: 0, z: 4000 } });
    promise.then(() => { r.pending = "resolved"; }, () => { r.pending = "rejected"; });
    world.tickingAreaManager.removeTickingArea("test:pending");
    r.blockedTeleport = cow.tryTeleport({ x: 3000, y: 200, z: 3000 });
    r.teleport = cow.tryTeleport({ x: 3, y: 200, z: 3 }, { rotation: { x: 0, y: 90 }, checkForBlocks: false })
        + ":" + cow.location.y + ":" + cow.getRotation().y;
    r.teleportBounds = (() => { try { cow.teleport({ x: 0, y: 100000, z: 0 }); return "ok"; }
                                catch (e) { return e.name; } })();
    cow.setDynamicProperties({ a: 1, b: "two" });
    r.entityDynamic = cow.getDynamicProperty("b") + ":" + (cow.getDynamicPropertyTotalByteCount() > 0);
    r.resetProperty = typeof cow.resetProperty;
    r.matchesLevel = player.matches({ minLevel: 0, maxLevel: 0 }) + "," + player.matches({ minLevel: 1 }) + ","
        + cow.matches({ minLevel: 0 }) + "," + cow.matches({ minVerticalRotation: -90, maxVerticalRotation: 90 });

    r.player = [player.isFlying, player.isGliding, player.isJumping, player.getTotalXp(),
                player.xpEarnedAtCurrentLevel, player.totalXpNeededForNextLevel, player.commandPermissionLevel,
                player.playerPermissionLevel].join(",");
    r.input = player.inputInfo.lastInputModeUsed + ":" + player.inputInfo.getButtonState(InputButton.Jump) + ":"
        + JSON.stringify(player.inputInfo.getMovementVector());
    player.selectedSlotIndex = 4;
    r.slot = player.selectedSlotIndex + ":" + (() => { try { player.selectedSlotIndex = 9; return "ok"; }
                                                    catch (e) { return e.name; } })();
    r.spawnBefore = player.getSpawnPoint() === undefined;
    player.setSpawnPoint({ dimension: overworld, x: 5, y: 70, z: 6 });
    const spawn = player.getSpawnPoint();
    r.spawn = spawn.x + "," + spawn.y + "," + spawn.z + "," + (spawn.dimension === overworld);
    player.setSpawnPoint();
    r.spawnCleared = player.getSpawnPoint() === undefined;
    r.playerFamilies = player.getComponent("type_family").getTypeFamilies().join(",")
        + ":" + player.matches({ families: ["player"] });
    r.playerHealth = player.getComponent("health").effectiveMax + ":" + player.hasComponent("player.hunger");
    const overrideError = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ": " + e.message; } };
    r.addItem = overrideError(() => cow.addItem(new ItemStack("minecraft:stick"))) + "|"
        + (player.addItem(new ItemStack("minecraft:stick")) === undefined);
    r.overrides = overrideError(() => player.setPropertyOverrideForEntity(cow, "test:none", 1)) + "|"
        + overrideError(() => player.clearPropertyOverridesForEntity(cow));
    const fog = player.fogSettings;
    r.fog = [fog.push("minecraft:fog_bamboo_jungle"), fog.push("Test:Mist", "a"), fog.push("test:haze", "a"),
             overrideError(() => fog.push("bad id")), fog.getStack().join(","), fog.getTags().join(","), fog.pop("a"),
             fog.pop(), fog.remove("none"), fog.remove(), fog.pop(),
             overrideError(() => fog.setStack(new Array(17).fill("a:b")))].join("|");
    fog.setStack(["a:b", "c:d"], "t");
    r.fogStack = fog.getStack().join(",") + "/" + fog.getTags().join(",");
    fog.remove();
    fog.push("test:fog");
    for (let i = 0; i < 10; i++)
        fog.push("test:fog", "t" + i);
    r.fogTags = fog.getTags().join(",");
    const scheme = (value) => overrideError(() => player.setControlScheme(value));
    r.controlScheme = [player.getControlScheme(), scheme("CameraRelative")];
    player.camera.setCamera("minecraft:first_person");
    r.controlScheme.push(scheme("CameraRelative"), scheme("Sideways"));
    player.camera.setCamera("minecraft:free");
    r.controlScheme.push(scheme("PlayerRelative"), player.getControlScheme(), scheme(), player.getControlScheme());
    r.controlScheme = r.controlScheme.join("|");
    const linear = new LinearSpline();
    linear.controlPoints = [{ x: 0, y: 1, z: 2 }];
    const frames = (progress, rotation) => ({ totalTimeSeconds: 2, animation: {
        progressKeyFrames: progress.map(([alpha, timeSeconds]) => ({ alpha, timeSeconds })),
        rotationKeyFrames: rotation.map((timeSeconds) => ({ rotation: { x: 0, y: 0, z: 0 }, timeSeconds })) } });
    const play = (spline, options) => overrideError(() => player.camera.playAnimation(spline, options));
    r.spline = [linear instanceof LinearSpline, linear.controlPoints[0].z, play(linear, frames([[0, 0]], [])),
                play(new CatmullRomSpline(), frames([], [])), play(linear, frames([[0, 1]], [])),
                play(linear, frames([], [0, 0.04])), play(linear, frames([[2, 1], [0, 0]], [1, 0]))].join("|");
    linear.controlPoints = [{ x: 0, y: 0, z: 0 }, { x: 4, y: 0, z: 0 }];
    r.splinePlay = play(linear, frames([[0, 0], [5, 1.5]], [0, 1])) + "|"
        + play({}, frames([], [])) + "|" + overrideError(() => { linear.controlPoints = [1]; });
    const device = player.clientSystemInfo;
    r.device = [device.memoryTier, device.maxRenderDistance, device.platformType, device.locale, player.graphicsMode,
                player.inputInfo.touchOnlyAffectsHotbar, typeof system.serverSystemInfo.memoryTier,
                Object.getPrototypeOf(Object.getPrototypeOf(device)) === Object.getPrototypeOf(system.serverSystemInfo)]
        .join(":");
    player.setGameMode("Adventure");
    r.gameMode = player.getGameMode();

    const manager = world.tickingAreaManager;
    r.tickingClass = manager instanceof TickingAreaManager;
    manager.createTickingArea("test:area", { dimension: overworld, from: { x: 0, y: 0, z: 0 }, to: { x: 20, y: 0, z: 5 } });
    r.ticking = manager.hasTickingArea("test:area") + ":" + manager.getAllTickingAreas().length + ":"
        + manager.getTickingArea("test:area").chunkCount + ":" + manager.chunkCount;
    r.tickingDuplicate = (() => { try { manager.createTickingArea("test:area", { dimension: overworld,
        from: { x: 0, y: 0, z: 0 }, to: { x: 0, y: 0, z: 0 } }); return "ok"; } catch (e) { return e.name + ":" + e.reason; } })();
    const areaError = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ":" + e.reason; } };
    r.tickingLimits = manager.maxChunkCount + ":"
        + areaError(() => manager.createTickingArea("test:wide", { dimension: overworld,
            from: { x: 0, y: 0, z: 0 }, to: { x: 256 * 16, y: 0, z: 0 } })) + ":"
        + areaError(() => manager.createTickingArea("test:many", { dimension: overworld,
            from: { x: 0, y: 0, z: 0 }, to: { x: 40 * 16, y: 0, z: 40 * 16 } })) + ":"
        + manager.hasCapacity({ dimension: overworld, from: { x: 0, y: 0, z: 0 },
            to: { x: 40 * 16, y: 0, z: 40 * 16 } })
        + ":" + manager.hasCapacity({ dimension: overworld, from: { x: 0, y: 0, z: 0 }, to: { x: 16, y: 0, z: 16 } })
        + ":" + areaError(() => manager.createTickingArea("test:far", { dimension: overworld,
            from: { x: 0, y: 0, z: 0 }, to: { x: 1e300, y: 0, z: 0 } })) + ":" + manager.hasTickingArea("test:wide");
    manager.removeTickingArea("test:area");
    r.tickingRemoved = manager.hasTickingArea("test:area");
    const propertyError = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ": " + e.message; } };
    r.propertyChecks = [propertyError(() => cow.setProperty("test:none", 1)),
                        propertyError(() => cow.setProperty("minecraft:climate_variant", 3)),
                        propertyError(() => cow.setProperty("minecraft:climate_variant", "hot")),
                        propertyError(() => cow.setProperty("minecraft:climate_variant", "warm")),
                        cow.getProperty("minecraft:climate_variant")].join("|");
    r.killed = cow.kill();
  } catch (e) { r.error = e.name + ":" + e.message + ":" + e.stack; }
    world.setDynamicProperty("entities", JSON.stringify(r));
}, 2);
system.runTimeout(() => {
  try {
    const sb = world.scoreboard;
    const overworld = world.getDimension("overworld");
    const player = world.getPlayers()[0];
    const calf = overworld.getEntities({ type: "minecraft:cow" })[0];
    r.sbClasses = (sb instanceof Scoreboard) + "," + (sb === world.scoreboard) + ","
        + capture(() => new ScoreboardObjective());
    r.sbNone = player.scoreboardIdentity === undefined;
    const obj = sb.addObjective("test:kills", "Kills");
    r.sbObjective = obj.id + "," + obj.displayName + "," + obj.isValid + "," + sb.getObjective("test:kills").id + ","
        + sb.getObjective("nope");
    r.sbDuplicate = capture(() => sb.addObjective("test:kills"));
    obj.setScore(player, 5);
    r.sbAdd = obj.addScore(player, 2) + "," + obj.addScore("fake", 3) + "," + obj.addScore(calf, -1);
    const id = player.scoreboardIdentity;
    r.sbIdentity = (id instanceof ScoreboardIdentity) + "," + id.type + "," + id.displayName + "," + id.isValid + ","
        + (id.getEntity() === player) + "," + typeof id.id;
    r.sbCalf = calf.scoreboardIdentity.type + "," + JSON.stringify(calf.scoreboardIdentity.displayName);
    r.sbScores = obj.getScores().map((s) => (s instanceof ScoreboardScoreInfo) + ":" + s.participant.type + "="
        + s.score).join(";");
    r.sbGet = obj.getScore(id) + "," + obj.getScore("nobody") + "," + obj.hasParticipant("fake") + ","
        + obj.hasParticipant(player);
    r.sbOverflow = capture(() => obj.addScore("fake", 2147483647));
    r.sbQuery = [overworld.getEntities({ scoreOptions: [{ objective: "test:kills", minScore: 0 }] }).length,
                 overworld.getEntities({ scoreOptions: [{ objective: "test:kills", maxScore: 0 }] }).length,
                 overworld.getEntities({ scoreOptions: [{ objective: "test:kills", minScore: 0, maxScore: 10,
                                                          exclude: true }] }).length,
                 world.getPlayers({ scoreOptions: [{ objective: "test:kills", minScore: 7, maxScore: 7 }] }).length,
                 overworld.getEntities({ scoreOptions: [{ objective: "test:none" }] }).length].join(",");
    const previous = sb.setObjectiveAtDisplaySlot(DisplaySlotId.Sidebar,
                                                  { objective: obj, sortOrder: ObjectiveSortOrder.Ascending });
    const shown = sb.getObjectiveAtDisplaySlot(DisplaySlotId.Sidebar);
    r.sbDisplay = (previous === undefined) + "," + shown.objective.id + "," + shown.sortOrder;
    r.sbParticipants = sb.getParticipants().length + "," + obj.getParticipants().length;
    r.sbRemoveParticipant = obj.removeParticipant("fake") + "," + obj.removeParticipant("fake");
    r.sbBadSlot = capture(() => sb.getObjectiveAtDisplaySlot("Feet"));
    r.sbClear = sb.clearObjectiveAtDisplaySlot(DisplaySlotId.Sidebar).id + ","
        + (sb.getObjectiveAtDisplaySlot(DisplaySlotId.Sidebar) === undefined);
    const other = sb.addObjective("test:other");
    r.sbRemove = sb.removeObjective(other) + "," + other.isValid + "," + capture(() => other.id) + ","
        + sb.removeObjective("nope");
    r.sbStale = capture(() => sb.setObjectiveAtDisplaySlot(DisplaySlotId.List, { objective: other }));
    sb.addObjective("test:other");
    r.sbReAdded = other.isValid;
    r.sbEnums = [DisplaySlotId.BelowName, ObjectiveSortOrder.Descending, ScoreboardIdentityType.FakePlayer].join(",");
    sb.setObjectiveAtDisplaySlot(DisplaySlotId.BelowName, { objective: obj });
    obj.setScore("kept", 42);
  } catch (e) { r.sbError = e.name + ":" + e.message + ":" + e.stack; }
}, 3);
system.runTimeout(() => world.setDynamicProperty("entities", JSON.stringify(r)), 7);
)JS";

    const char *const GRAPHICS = R"JS(
import { world, system, BiomeTypes } from "@minecraft/server";
import { BiomeWater, getBiomeAtmospherics, getBiomeColorGrading, getBiomeLighting,
         getPlayerWater } from "@minecraft/server-graphics";

const r = {};
const errorOf = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "gfx:apply") return;
    const plains = BiomeTypes.get("minecraft:plains");
    const p = world.getAllPlayers()[0];
    const sky = getBiomeAtmospherics(plains);
    sky.setSkyZenithColor({ red: 1, green: 0, blue: 0.5 });
    sky.setHorizonBlendMax({ 0: 0.2, 0.5: 0.8 });
    getPlayerWater(plains, p).setWavesDepth(3);
    getBiomeColorGrading(plains).setHighlightsGain({ x: 1, y: 2, z: 3 });
    getBiomeLighting(plains).resetSunColor();
    r.range = errorOf(() => sky.setHorizonBlendMax(2));
    r.color = errorOf(() => sky.setSkyZenithColor({ red: 2, green: 0, blue: 0 }));
    r.key = errorOf(() => sky.setHorizonBlendMax({ 2: 0.5 }));
    r.type = errorOf(() => sky.setHorizonBlendMax("x"));
    r.biome = errorOf(() => getBiomeAtmospherics({}));
    r.constructor = errorOf(() => new BiomeWater());
    world.setDynamicProperty("graphics", JSON.stringify(r));
});
)JS";

    const char *const LOOT = R"JS(
import { world, system, ItemStack, LootTable, LootPool, LootItem, LootTableManager, BlockPermutation, BlockTypes,
         EntityTypes } from "@minecraft/server";

const r = {};
const errorOf = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
system.runTimeout(() => {
    const m = world.getLootTableManager();
    r.manager = m instanceof LootTableManager && m === world.getLootTableManager();
    const t = m.getLootTable("entities/zombie");
    r.zombie = t.path + "|" + t.pools.length + "|" + (t.pools[0] instanceof LootPool) + "|"
        + t.pools[0].rolls.min + "-" + t.pools[0].rolls.max;
    const entry = t.pools[0].entries[0];
    r.entry = (entry instanceof LootItem) + "|" + entry.name.id + "|" + entry.weight + "|"
        + entry.functions.map((f) => f.constructor.name + ":" + f.count.min + "-" + f.count.max).join(",");
    r.conditions = t.pools[1].conditions.map((c) => c.constructor.name).join(",") + "|"
        + t.pools[1].conditions[1].lootingMultiplier;
    r.missing = String(m.getLootTable("entities/nope"));
    r.roll = m.generateLootFromTable(t).every((i) => i.typeId === "minecraft:rotten_flesh" || i instanceof ItemStack);
    r.dirt = m.generateLootFromBlockPermutation(BlockPermutation.resolve("minecraft:dirt"))
        .map((i) => i.typeId + "x" + i.amount).join(",");
    r.stoneHand = String(m.generateLootFromBlockType(BlockTypes.get("minecraft:stone")));
    r.stonePick = m.generateLootFromBlockType(BlockTypes.get("minecraft:stone"),
        new ItemStack("minecraft:wooden_pickaxe")).map((i) => i.typeId).join(",");
    r.entityType = Array.isArray(m.generateLootFromEntityType(EntityTypes.get("minecraft:zombie")));
    r.tool = errorOf(() => m.generateLootFromTable(t, {}));
    r.constructor = errorOf(() => new LootTable());
    const gem = m.getLootTable("custom/gem");
    r.pack = gem.path + "|" + m.generateLootFromTable(gem).map((i) => i.typeId + "x" + i.amount).join(",");
    r.override = m.generateLootFromTable(m.getLootTable("chests/igloo_chest")).map((i) => i.typeId).join(",");
    world.setDynamicProperty("loot", JSON.stringify(r));
}, 2);
)JS";

    const char *const SOUNDS = R"JS(
import { world, system, SoundInstance, SoundDurationInfo, SoundDefinition, SoundDefinitionRegistry }
    from "@minecraft/server";

const r = {};
const errorOf = (run) => { try { run(); return "ok"; } catch (e) { return e.name; } };
system.runTimeout(() => {
  try {
    const registry = world.soundDefinitionRegistry;
    r.registry = (registry instanceof SoundDefinitionRegistry) + ":" + (registry === world.soundDefinitionRegistry);
    const all = registry.getDefinitions();
    r.definitions = all.map((d) => d.soundEventId).join(",") + ":" + (all[0] instanceof SoundDefinition);
    const jingle = all[0];
    r.jingle = jingle.durationInfo.duration + ":" + jingle.musicInfo.artist + ":" + jingle.musicInfo.genres.join("|")
        + ":" + jingle.musicInfo.title + ":" + String(jingle.musicInfo.moods) + ":" + jingle.tags.mood.join("|");
    r.plain = String(all[1].durationInfo) + ":" + String(all[1].musicInfo) + ":" + String(all[1].tags);
    r.quantized = all[2].durationInfo.duration + ":" + all[3].durationInfo.duration;
    r.filters = registry.getDefinitions({ genres: ["POP"] }).length + ":"
        + registry.getDefinitions({ minDuration: 1 }).length + ":" + registry.getDefinitions({ maxDuration: 1 }).length
        + ":" + registry.getDefinitions({ tags: { MOOD: ["Calm"] } }).length + ":"
        + registry.getDefinitions({ artists: ["nobody"] }).length + ":"
        + errorOf(() => registry.getDefinitions({ minDuration: 2, maxDuration: 1 }));

    const player = world.getPlayers()[0];
    const sound = player.playSound(jingle, { loopCount: 1 });
    world.afterEvents.soundCompleted.subscribe((e) => {
        r.completed = (e.soundInstanceId === sound.id) + ":" + sound.durationInfo.isActive + ":"
            + sound.durationInfo.getPlaybackPosition();
        world.setDynamicProperty("sounds", JSON.stringify(r));
    });
    r.instance = (sound instanceof SoundInstance) + ":" + sound.soundEventId + ":" + (sound.recipient.id === player.id)
        + ":" + (sound.durationInfo instanceof SoundDurationInfo) + ":" + sound.durationInfo.duration + ":"
        + sound.durationInfo.isActive + ":" + sound.durationInfo.getPlaybackPosition();
    const orb = world.getDimension("overworld").playSound("random.orb", { x: 0, y: 64, z: 0 }, { isBroadcast: true });
    r.dimension = (orb instanceof SoundInstance) + ":" + String(orb.recipient) + ":" + String(orb.durationInfo) + ":"
        + (orb.id !== sound.id);
    r.bounds = errorOf(() => orb.setPitch(20)) + ":" + errorOf(() => orb.setVolume(-1)) + ":"
        + errorOf(() => sound.seekTo(1)) + ":" + errorOf(() => orb.fade(-1, 1)) + ":"
        + errorOf(() => world.getDimension("overworld").playSound("random.orb", { x: 0, y: 64, z: 0 }, { volume: -1 }))
        + ":" + errorOf(() => player.playSound("random.orb", { pitch: 0 }));
    r.controls = errorOf(() => { orb.setPitch(2); orb.setVolume(0.5); orb.fade(1, 0); orb.pause(); orb.resume();
        orb.seekTo(3); orb.stop(); });
    sound.seekTo(0.05);
    r.seek = sound.durationInfo.getPlaybackPosition().toFixed(2);
    const endless = player.playSound(all[2], { loopCount: -1 });
    r.endless = endless.durationInfo.isActive + ":" + endless.durationInfo.getPlaybackPosition() + ":"
        + /^[0-9]+$/.test(endless.id);
    endless.stop();
  } catch (e) {
    r.error = e.name + ":" + e.message + ":" + e.stack;
  }
    world.setDynamicProperty("sounds", JSON.stringify(r));
}, 2);
system.afterEvents.scriptEventReceive.subscribe((e) => {
  if (e.id !== "sounds:broadcast")
    return;
  const overworld = world.getDimension("overworld");
  overworld.playSound("test:ranged", { x: 2, y: 100, z: 0 }, { isBroadcast: true });
  overworld.playSound("test:ranged", { x: 9, y: 100, z: 0 }, { isBroadcast: true });
  overworld.playSound("random.orb", { x: 20, y: 100, z: 0 }, { isBroadcast: true });
  overworld.playSound("random.orb", { x: 5000, y: 100, z: 0 }, { isBroadcast: true });
  overworld.playSound("random.pop", { x: 5000, y: 100, z: 0 });
});
)JS";

    const char *const ITEMS = R"JS(
import { world, system, ItemStack, ItemType, ItemTypes, Container, ContainerSlot, EnchantmentType, EnchantmentTypes,
         EnchantmentSlot, ItemLockMode, Potions, PotionEffectType, ItemDurabilityComponent, ItemEnchantableComponent,
         ItemFoodComponent, ItemBookComponent, ItemPotionComponent, ItemInventoryComponent, ItemCooldownComponent,
         EntityInventoryComponent, EntityEquippableComponent, EntityEnderInventoryComponent,
         PlayerCursorInventoryComponent, BlockTypes, BlockPermutation, InvalidItemStackError } from "@minecraft/server";

const log = (line) => { r.events = (r.events || "") + line + ";"; };
const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name; } };
world.afterEvents.itemStartUse.subscribe((e) => log("start:" + e.itemStack.typeId + ":" + e.useDuration));
world.afterEvents.itemStopUse.subscribe((e) => log("stop:" + e.itemStack.typeId + ":" + e.useDuration));
world.afterEvents.itemReleaseUse.subscribe((e) => log("release:" + e.itemStack.typeId + ":" + e.useDuration));
world.afterEvents.itemCompleteUse.subscribe((e) => log("complete:" + e.itemStack.typeId + ":" + e.source.name));
world.beforeEvents.entityItemPickup.subscribe((e) => {
    log("beforePickup:" + e.entity.typeId + ":" + e.item.typeId);
    if (e.entity.typeId === "minecraft:piglin")
        e.cancel = true;
});
world.afterEvents.entityItemPickup.subscribe((e) => log("pickup:" + e.entity.typeId + ":"
    + e.items.map((i) => i.typeId + "x" + i.amount).join(",")), { itemFilter: { includeTypes: ["diamond"] } });
world.afterEvents.entityItemPickup.subscribe((e) => log("filteredPickup"), { itemFilter: { includeTypes: ["stone"] } });
world.afterEvents.playerInventoryItemChange.subscribe((e) => log("change:" + e.slot + ":" + e.inventoryType + ":"
    + (e.beforeItemStack ? e.beforeItemStack.typeId : "-") + ">" + (e.itemStack ? e.itemStack.typeId : "-")),
    { includeItems: ["minecraft:diamond"] });
world.afterEvents.entityContainerOpened.subscribe((e) => log("opened:" + e.entity.typeId + ":"
    + e.openSource.entity.typeId), { entityFilter: { type: "minecraft:chest_minecart" } });
world.afterEvents.entityContainerOpened.subscribe((e) => log("wrongOpened"), { entityFilter: { type: "minecraft:cow" } });
world.afterEvents.entityContainerClosed.subscribe((e) => log("closed:" + e.entity.typeId + ":"
    + e.closeSource.entity.typeId), { accessSourceFilter: { entityFilter: { type: "minecraft:player" } } });
world.afterEvents.playerInventoryItemChange.subscribe((e) => log("hotbar:" + e.slot),
    { inventoryType: "Hotbar", allowedSlots: [3] });
const worldLog = (line) => { r.world = (r.world || "") + line + ";"; };
world.afterEvents.blockContainerOpened.subscribe((e) => worldLog("opened:" + e.block.typeId + ":"
    + e.openSource.entity.typeId + ":" + (e.dimension.id === "minecraft:overworld")),
    { blockFilter: { includeTypes: ["minecraft:stone"] } });
world.afterEvents.blockContainerOpened.subscribe(() => worldLog("wrongOpened"),
    { blockFilter: { includeTypes: ["minecraft:barrel"] } });
world.afterEvents.blockContainerClosed.subscribe((e) => worldLog("closed:" + e.block.typeId + ":"
    + e.closeSource.entity.typeId), { accessSourceFilter: { entityFilter: { type: "minecraft:player" } } });
world.afterEvents.playerStartBreakingBlock.subscribe((e) => worldLog("start:" + e.blockPermutation.type.id + ":"
    + e.face + ":" + e.player.typeId));
world.afterEvents.playerStartBreakingBlock.subscribe(() => worldLog("wrongStart"),
    { playerFilter: { type: "minecraft:cow" } });
world.afterEvents.playerCancelBreakingBlock.subscribe((e) => worldLog("cancel:" + e.block.typeId + ":"
    + e.breakProgress));
world.afterEvents.itemStartUseOn.subscribe((e) => worldLog("useOn:" + e.block.typeId + ":" + e.blockFace + ":"
    + e.source.typeId));
world.afterEvents.itemStopUseOn.subscribe((e) => worldLog("stopUseOn:" + e.block.typeId));
world.afterEvents.playerInputPermissionCategoryChange.subscribe((e) => r.input = (r.input || "") + e.category
    + ":" + e.enabled + ":" + e.player.typeId + ";");

system.runTimeout(() => {
  try {
    const player = world.getPlayers()[0];
    const sword = new ItemStack("diamond_sword");
    r.constructed = sword.typeId + ":" + sword.amount + ":" + sword.maxAmount + ":" + sword.isStackable + ":"
        + (sword.type instanceof ItemType) + ":" + sword.type.id;
    r.badType = fail(() => new ItemStack("minecraft:nope"));
    r.badAmount = fail(() => new ItemStack("stone", 256));
    r.clamped = new ItemStack("ender_pearl", 40).amount;
    sword.nameTag = "Blade";
    sword.setLore(["first", { rawtext: [{ text: "second" }] }]);
    sword.keepOnDeath = true;
    sword.lockMode = ItemLockMode.slot;
    r.meta = sword.nameTag + ":" + sword.getLore().length + ":" + sword.getRawLore()[1].rawtext[0].text + ":"
        + sword.getRawLore()[0].text + ":" + sword.keepOnDeath + ":" + sword.lockMode;
    r.loreBounds = fail(() => sword.setLore(["x".repeat(51)]));
    sword.setCanDestroy(["stone"]);
    r.canDestroy = sword.getCanDestroy().join(",") + ":" + fail(() => sword.setCanDestroy(["nope:block"]));
    sword.setDynamicProperty("test:owner", "me");
    r.dynamic = sword.getDynamicProperty("test:owner") + ":" + fail(() => new ItemStack("stone").setDynamicProperty("a", 1));
    const copy = sword.clone();
    copy.nameTag = undefined;
    r.clone = copy.getLore().length + ":" + (copy.nameTag === undefined) + ":" + sword.nameTag + ":"
        + copy.getDynamicProperty("test:owner");
    r.stackable = new ItemStack("stone").isStackableWith(new ItemStack("stone", 5)) + ":"
        + sword.isStackableWith(copy) + ":" + new ItemStack("stone").matches("stone");

    const durability = sword.getComponent("durability");
    durability.damage = 10;
    r.durability = (durability instanceof ItemDurabilityComponent) + ":" + durability.maxDurability + ":"
        + sword.getComponent(ItemDurabilityComponent.componentId).damage + ":" + fail(() => { durability.damage = 99999; })
        + ":" + fail(() => durability.getDamageChance(4));
    const enchantable = sword.getComponent("minecraft:enchantable");
    enchantable.addEnchantment({ type: new EnchantmentType("sharpness"), level: 3 });
    r.enchant = (enchantable instanceof ItemEnchantableComponent) + ":" + enchantable.getEnchantments().length + ":"
        + enchantable.getEnchantment("minecraft:sharpness").level + ":" + enchantable.hasEnchantment("unbreaking") + ":"
        + enchantable.slots.join("|");
    r.enchantErrors = [fail(() => enchantable.addEnchantment({ type: "nope", level: 1 })),
        fail(() => enchantable.addEnchantment({ type: "sharpness", level: 9 })),
        fail(() => enchantable.addEnchantment({ type: "power", level: 1 })),
        enchantable.canAddEnchantment({ type: "smite", level: 1 }),
        enchantable.canAddEnchantment({ type: "unbreaking", level: 1 })].join(",");
    r.enchantTypes = EnchantmentTypes.get("minecraft:flame").id + ":" + EnchantmentTypes.get("flame").maxLevel + ":"
        + (EnchantmentTypes.getAll().length > 30) + ":" + (EnchantmentTypes.get("nope") === undefined) + ":"
        + EnchantmentSlot.Sword;
    r.components = sword.getComponents().map((c) => c.typeId).sort().join(",");

    const apple = new ItemStack("apple");
    const food = apple.getComponent("food");
    r.food = (food instanceof ItemFoodComponent) + ":" + food.nutrition + ":" + food.saturationModifier.toFixed(1)
        + ":" + food.canAlwaysEat + ":" + (apple.getComponent("durability") === undefined);
    const pearl = new ItemStack("ender_pearl").getComponent("cooldown");
    r.cooldown = (pearl instanceof ItemCooldownComponent) + ":" + pearl.cooldownCategory + ":"
        + pearl.isCooldownCategory(pearl.cooldownCategory);

    const potion = Potions.resolve("minecraft:swiftness", "ThrownSplash");
    const potionComponent = potion.getComponent("potion");
    r.potion = potion.typeId + ":" + (potionComponent instanceof ItemPotionComponent) + ":"
        + potionComponent.potionEffectType.id + ":" + potionComponent.potionDeliveryType.id + ":"
        + Potions.getEffectType("minecraft:swiftness").durationTicks + ":"
        + (Potions.getEffectType("minecraft:healing").durationTicks === undefined) + ":"
        + Potions.getAllDeliveryTypes().length + ":" + fail(() => Potions.resolve("nope", "Consume"));

    const book = new ItemStack("writable_book");
    const pages = book.getComponent("book");
    pages.setContents(["one", "two"]);
    pages.insertPage(1, "inserted");
    pages.setPageContent(4, { rawtext: [{ text: "raw" }] });
    r.bookBefore = (pages instanceof ItemBookComponent) + ":" + pages.pageCount + ":" + pages.getPageContent(1) + ":"
        + (pages.getPageContent(4) === undefined) + ":" + pages.getRawPageContent(4).rawtext[0].text + ":"
        + pages.isSigned + ":" + fail(() => pages.setPageContent(0, "x".repeat(257)));
    r.bookTitle = (() => { try { pages.signBook("x".repeat(17), "me"); return "ok"; } catch (e) { return e.name + ":" + e.reason; } })();
    pages.signBook("Story", "ScriptPlayer");
    r.bookSigned = book.typeId + ":" + pages.isSigned + ":" + pages.title + ":" + pages.author;

    const bundle = new ItemStack("bundle");
    const storage = bundle.getComponent("inventory");
    storage.container.addItem(new ItemStack("stone", 3));
    r.bundle = (storage instanceof ItemInventoryComponent) + ":" + storage.container.getItem(0).amount + ":"
        + bundle.getComponent("inventory").container.firstEmptySlot() + ":" + storage.container.weight + ":"
        + storage.container.containerRules.bannedItems.length;

    const inventory = player.getComponent("inventory");
    const container = inventory.container;
    container.clearAll();
    r.inventory = (inventory instanceof EntityInventoryComponent) + ":" + (container instanceof Container) + ":"
        + container.size + ":" + container.emptySlotsCount + ":" + inventory.inventorySize;
    const rest = container.addItem(new ItemStack("ender_pearl", 16));
    container.setItem(5, sword);
    container.setItem(3, new ItemStack("diamond", 2));
    container.setItem(12, new ItemStack("diamond", 1));
    r.container = (rest === undefined) + ":" + container.getItem(5).nameTag + ":" + container.firstItem() + ":"
        + container.find(new ItemStack("diamond", 1)) + ":" + container.findLast(new ItemStack("diamond", 2)) + ":"
        + container.contains(new ItemStack("diamond", 2)) + ":" + fail(() => container.getItem(36));
    const slot = container.getSlot(5);
    slot.nameTag = "Renamed";
    slot.amount = 1;
    r.slot = (slot instanceof ContainerSlot) + ":" + slot.hasItem() + ":" + container.getItem(5).nameTag + ":"
        + slot.typeId + ":" + slot.getComponent + ":" + slot.isValid + ":" + slot.getLore().length;
    container.swapItems(3, 12, container);
    container.moveItem(12, 20, container);
    r.moved = container.getItem(3).amount + ":" + (container.getItem(12) === undefined) + ":" + container.getItem(20).amount;
    const transferred = container.transferItem(20, bundle.getComponent("inventory").container);
    r.transferred = (transferred === undefined) + ":" + (container.getItem(20) === undefined);

    const equipment = player.getComponent("equippable");
    equipment.setEquipment("Head", new ItemStack("diamond_helmet"));
    equipment.setEquipment("Offhand", new ItemStack("shield"));
    r.equipment = (equipment instanceof EntityEquippableComponent) + ":" + equipment.getEquipment("Head").typeId + ":"
        + equipment.getEquipmentSlot("Offhand").typeId + ":" + equipment.totalArmor + ":" + equipment.totalToughness;
    const ender = player.getComponent("minecraft:ender_inventory");
    ender.container.setItem(0, new ItemStack("emerald", 4));
    r.ender = (ender instanceof EntityEnderInventoryComponent) + ":" + ender.container.size + ":"
        + player.getComponent("ender_inventory").container.getItem(0).amount;
    const cursor = player.getComponent("cursor_inventory");
    r.cursor = (cursor instanceof PlayerCursorInventoryComponent) + ":" + (cursor.item === undefined);
    r.playerComponents = player.getComponents().map((c) => c.typeId).filter((id) => id.endsWith("inventory")
        || id === "minecraft:equippable").sort().join(",");

    const piglin = world.getDimension("overworld").getEntities({ type: "minecraft:piglin" })[0];
    const mobInventory = piglin.getComponent("inventory");
    mobInventory.container.setItem(1, new ItemStack("gold_ingot", 3));
    piglin.getComponent("equippable").setEquipment("Mainhand", new ItemStack("golden_sword"));
    r.mob = mobInventory.inventorySize + ":" + mobInventory.containerType + ":" + mobInventory.private + ":"
        + mobInventory.container.getItem(1).amount + ":" + piglin.getComponent("equippable").getEquipment("Mainhand").typeId
        + ":" + (world.getDimension("overworld").getEntities({ type: "minecraft:cow" })[0].getComponent("inventory") === undefined);
    r.itemTypes = (ItemTypes.get("apple") === new ItemStack("apple").type) + ":" + new ItemStack("apple").localizationKey;

    const tool = new ItemStack("iron_pickaxe");
    tool.setDynamicProperties({ "test:a": 1, "test:b": "two" });
    r.dynamicMore = tool.getDynamicPropertyIds().sort().join(",") + ":" + (tool.getDynamicPropertyTotalByteCount() > 0);
    tool.clearDynamicProperties();
    tool.setCanPlaceOn(["minecraft:dirt"]);
    const toolDurability = tool.getComponent("durability");
    toolDurability.unbreakable = true;
    r.toolMore = tool.getDynamicPropertyIds().length + ":" + tool.getCanPlaceOn().join(",") + ":"
        + toolDurability.unbreakable + ":" + JSON.stringify(toolDurability.getDamageChanceRange()) + ":"
        + toolDurability.getDamageChance(1) + ":" + tool.matches("minecraft:iron_pickaxe") + ":" + tool.matches("stone");
    const toolEnchant = tool.getComponent("enchantable");
    toolEnchant.addEnchantments([{ type: "efficiency", level: 2 }, { type: "unbreaking", level: 1 }]);
    toolEnchant.removeEnchantment("efficiency");
    r.enchantMore = toolEnchant.getEnchantments().map((e) => e.type.id + e.level).join(",");
    toolEnchant.removeAllEnchantments();
    r.enchantCleared = toolEnchant.getEnchantments().length + ":" + toolEnchant.slots.join("|");
    const leather = new ItemStack("leather_helmet").getComponent("dyeable");
    // no vanilla item declares minecraft:dyeable, leather armor included
    r.dye = (leather === undefined) + ":" + (new ItemStack("stone").getComponent("dyeable") === undefined);
    r.compost = new ItemStack("red_shrub").getComponent("compostable").compostingChance;
    // the book component only references its stack: keep the stack alive
    const quillStack = new ItemStack("writable_book");
    const quill = quillStack.getComponent("book");
    quill.setContents(["a", { rawtext: [{ text: "b" }] }, "c"]);
    quill.removePage(2);
    r.bookMore = quill.contents.map((page) => page === undefined ? "~" : page).join(",") + ":"
        + quill.rawContents.map((page) => page === undefined ? "~" : page.rawtext[0].text).join(",") + ":"
        + (quill.title === undefined) + ":" + (quill.author === undefined);
    r.potionsMore = Potions.getAllEffectTypes().length + ":" + Potions.getDeliveryType("Consume").id + ":"
        + (Potions.getDeliveryType("nope") === undefined) + ":" + Potions.resolve("minecraft:healing", "Consume").typeId;
    const pearl2 = new ItemStack("ender_pearl").getComponent("cooldown");
    pearl2.startCooldown(player);
    r.cooldownMore = pearl2.cooldownTicks + ":" + (pearl2.getCooldownTicksRemaining(player) > 0);
    const playerInventory = player.getComponent("inventory");
    r.inventoryMore = playerInventory.containerType + ":" + playerInventory.private + ":"
        + playerInventory.canBeSiphonedFrom + ":" + playerInventory.restrictToOwner + ":"
        + playerInventory.additionalSlotsPerStrength + ":" + (playerInventory.container.containerRules === undefined);
    r.rules = (() => { try { new ItemStack("bundle").getComponent("inventory").container.addItem(new ItemStack("shulker_box"));
        return "ok"; } catch (e) { return e.name + ":" + e.reason; } })();
    const cart = world.getDimension("overworld").getEntities({ type: "minecraft:chest_minecart" })[0];
    const cartInventory = cart.getComponent("inventory");
    cartInventory.container.setItem(4, new ItemStack("diamond", 7));
    r.cart = cartInventory.inventorySize + ":" + cartInventory.containerType + ":" + cartInventory.canBeSiphonedFrom
        + ":" + cartInventory.container.size + ":" + cartInventory.container.getItem(4).amount + ":"
        + (cart.getComponent("equippable") === undefined);
    const chestItem = new ItemStack("chest");
    const blockProperties = chestItem.getComponent("minecraft:block_actor_dynamic_properties");
    blockProperties.set("test:key", 5);
    const chestClone = chestItem.clone();
    r.blockProperties = blockProperties.get("test:key") + ":" + (blockProperties.totalByteCount() > 0) + ":"
        + chestClone.getComponent("block_actor_dynamic_properties").get("test:key") + ":"
        + (new ItemStack("apple").getComponent("block_actor_dynamic_properties") === undefined);
    blockProperties.set("test:key", undefined);
    // a component outlives its ItemStack only as a weak handle: every member then throws
    const deadBook = new ItemStack("writable_book").getComponent("book");
    const deadProperties = new ItemStack("chest").getComponent("block_actor_dynamic_properties");
    const invalid = (read) => { try { read(); return "no throw"; } catch (e) {
        return e.name + "|" + (e instanceof InvalidItemStackError) + "|" + e.itemType.id + "|" + e.message; } };
    r.deadBook = deadBook.isValid + ";" + deadBook.typeId + ";" + invalid(() => deadBook.pageCount) + ";"
        + invalid(() => deadBook.getPageContent(0));
    r.deadProperties = deadProperties.isValid + ";" + invalid(() => deadProperties.totalByteCount());
    r.liveBook = quill.isValid + ";" + quill.pageCount;
    r.blockPropertiesCleared = String(blockProperties.get("test:key"));
    const log = new ItemStack("oak_log");
    r.stateMatches = log.matches("oak_log", { pillar_axis: "y" }) + ":" + log.matches("oak_log", { pillar_axis: "x" })
        + ":" + log.matches("oak_log", { nope: 1 }) + ":" + new ItemStack("apple").matches("apple", { pillar_axis: "y" });
    const stoneBlock = world.getDimension("overworld").getBlock({ x: 0, y: 101, z: 8 });
    player.inputPermissions.setPermissionCategory(2, false);
    player.inputPermissions.setPermissionCategory(2, false);
    player.inputPermissions.setPermissionCategory(2, true);
    r.blockType = stoneBlock.type.id + ":" + (stoneBlock.type === BlockTypes.get("stone")) + ":"
        + (BlockPermutation.resolve("stone").type === BlockTypes.get("minecraft:stone"));
    const cursorComponent = player.getComponent("cursor_inventory");
    r.cursorMore = fail(() => cursorComponent.clear()) + ":" + (cursorComponent.item === undefined) + ":"
        + cursorComponent.isValid + ":" + (cursorComponent.entity === player);
  } catch (e) { r.error = e.name + ":" + e.message + ":" + e.stack; }
    world.setDynamicProperty("items", JSON.stringify(r));
}, 2);
system.runTimeout(() => world.setDynamicProperty("items", JSON.stringify(r)), 7);
)JS";

    const char *const BETA_ENTITIES = R"JS(
import { world, system, Entity, Player, ItemStack, BlockInstrumentComponent, BlockCustomComponentInstance,
         ItemCustomComponentInstance } from "@minecraft/server";
r.target = "target" in Entity.prototype;
world.afterEvents.playerCursorItemGrab.subscribe((e) => { r.cursor = (r.cursor || "") + "grab:" + e.item.typeId + ";"; });
world.afterEvents.playerCursorItemRelease.subscribe((e) => {
    r.cursor = (r.cursor || "") + "release:" + e.item.typeId + ":" + e.player.name + ";";
    world.setDynamicProperty("betaEntities", JSON.stringify(r));
});
r.betaPlayer = ["chatDisplayName", "chatNamePrefix", "eatItem", "postClientMessage", "stopSound", "stopAllSounds"]
    .every((key) => key in Player.prototype);
system.runTimeout(() => {
    const player = world.getPlayers()[0];
    player.chatNamePrefix = "[A] ";
    player.chatNameSuffix = "!";
    player.chatMessagePrefix = "§a";
    r.chat = player.chatDisplayName + ":" + player.chatMessagePrefix;
    player.chatNameSuffix = undefined;
    r.chatCleared = player.chatDisplayName + ":" + (player.chatNameSuffix === undefined);
    player.stopSound("random.click");
    player.stopAllSounds();
    player.postClientMessage("test:id", "value");
    const food = player.getComponent("player.hunger");
    food.setCurrentValue(10);
    player.eatItem(new ItemStack("minecraft:apple"));
    r.eat = food.currentValue > 10;
    r.eatStone = (() => { try { player.eatItem(new ItemStack("minecraft:stone")); return "ok"; }
                          catch (e) { return e.name; } })();
    r.ping = (() => { try { return String(player.getPing()); } catch (e) { return e.name; } })();
    const dimension = world.getDimension("overworld");
    dimension.setBlockType({ x: 1, y: 101, z: 8 }, "test:block");
    const instrument = dimension.getBlock({ x: 1, y: 101, z: 8 }).getComponent("instrument_sound");
    const failure = (run) => { try { run(); return "ok"; } catch (e) { return e.name; } };
    r.instrument = (instrument instanceof BlockInstrumentComponent) + ":" + instrument.getInstrumentName("Up") + ":"
        + instrument.getInstrumentName("Down") + ":" + failure(() => instrument.getInstrumentName("North")) + ":"
        + failure(() => instrument.getInstrumentName("up")) + ":"
        + failure(() => instrument.playInstrumentSound("Up", { volume: 0.5 })) + ":"
        + failure(() => instrument.playInstrumentSound("Down", { pitch: 0 })) + ":"
        + (dimension.getBlock({ x: 0, y: 101, z: 8 }).getComponent("instrument_sound") === undefined);
    const block = dimension.getBlock({ x: 1, y: 101, z: 8 });
    const custom = block.getComponent("test:hooks");
    const wand = new ItemStack("test:wand").getComponent("test:wand");
    r.customInstance = (custom instanceof BlockCustomComponentInstance) + ":"
        + JSON.stringify(custom.customComponentParameters.params) + ":" + (block.getComponent("test:none") === undefined)
        + ":" + block.getComponents().some((component) => component.typeId === "test:hooks") + ":"
        + (wand instanceof ItemCustomComponentInstance) + ":" + JSON.stringify(wand.customComponentParameters.params)
        + ":" + (new ItemStack("minecraft:apple").getComponent("test:wand") === undefined);
    // the default color only shows once the stack has a color of its own
    const cloakDye = new ItemStack("test:cloak").getComponent("dyeable");
    const undyed = (cloakDye.color === undefined) + ":" + (cloakDye.defaultColor === undefined);
    cloakDye.color = { red: 1, green: 0, blue: 0 };
    const channels = (rgb) => [rgb.red, rgb.green, rgb.blue].map((c) => Math.round(c * 255)).join(",");
    r.packDye = undyed + ":" + channels(cloakDye.color) + ":" + channels(cloakDye.defaultColor) + ":"
        + (new ItemStack("test:wand").getComponent("dyeable") === undefined);
    world.setDynamicProperty("betaEntities", JSON.stringify(r));
}, 3);
)JS";

    const char *const STABLE1 = R"JS(
import { world, system, GameMode } from "@minecraft/server";
import * as server from "@minecraft/server";

r.gameMode = String(GameMode.survival) + "," + String(GameMode.Survival);
r.worldInitialize = "worldInitialize" in world.beforeEvents;
r.worldLoad = "worldLoad" in world.afterEvents;
r.systemBefore = "beforeEvents" in system;
r.early = capture(() => world.getAllPlayers());
r.startupEvent = "StartupEvent" in server;
r.sbValidMethod = typeof server.ScoreboardObjective.prototype.isValid + ","
    + typeof server.ScoreboardIdentity.prototype.isValid;
world.beforeEvents.worldInitialize.subscribe((event) => {
    r.register = capture(() => event.blockComponentRegistry.registerCustomComponent("legacy:comp", {
        onPlayerInteract(...args) {
            r.legacyInteract = args.length + "|" + Object.getPrototypeOf(args[0]).constructor.name;
            world.setDynamicProperty("stable1", JSON.stringify(r));
        },
        onPlayerDestroy(event) {
            r.legacyDestroy = Object.keys(event).filter((k) => k !== "block" && k !== "dimension").sort().join("+");
            world.setDynamicProperty("stable1", JSON.stringify(r));
        },
        onPlayerBreak() {
            r.legacyBreak = "called";
        },
    }));
    r.legacyName = (() => {
        try {
            event.blockComponentRegistry.registerCustomComponent("nonamespace", {});
        } catch (e) {
            return e.name + "," + e.reason;
        }
        return "ok";
    })();
});
world.afterEvents.worldInitialize.subscribe(() => {
    world.setDynamicProperty("stable1", JSON.stringify(r));
});
)JS";

    const char *const COMPONENTS = R"JS(
import { world, system, BlockPermutation, CustomComponentParameters, BlockComponentPlayerInteractEvent,
         BlockEvent, ItemComponentUseOnEvent, ItemUseOnEvent, NamespaceNameError, WorldClock, Dimension } from "@minecraft/server";

const save = () => world.setDynamicProperty("components", JSON.stringify(r));
const describe = (event, params) => {
    const fields = Object.keys(event).filter((k) => k !== "block" && k !== "dimension").sort();
    return [Object.getPrototypeOf(event).constructor.name, fields.join("+"),
            params instanceof CustomComponentParameters ? JSON.stringify(params.params) : "none"].join("|");
};
const log = (name) => (event, params) => {
    r[name] = describe(event, params);
    save();
};

let placeCalls = 0;
system.beforeEvents.startup.subscribe((event) => {
    const blocks = event.blockComponentRegistry;
    const items = event.itemComponentRegistry;
    r.noNamespace = (() => {
        try {
            blocks.registerCustomComponent("nonamespace", {});
        } catch (e) {
            return (e instanceof NamespaceNameError) + "," + e.reason;
        }
        return "ok";
    })();
    r.minecraftNamespace = capture(() => items.registerCustomComponent("minecraft:thing", {}));
    r.notObject = capture(() => blocks.registerCustomComponent("test:bad", 3));
    r.registerBlock = capture(() => blocks.registerCustomComponent("test:hooks", {
        beforeOnPlayerPlace(event, params) {
            placeCalls++;
            r.beforePlace = describe(event, params) + "|" + event.permutationToPlace.type.id + "|" + event.face;
            if (placeCalls === 1)
                event.cancel = true;
            else
                event.permutationToPlace = BlockPermutation.resolve("minecraft:stone");
            save();
        },
        onPlace(event, params) {
            r.onPlace = describe(event, params) + "|" + event.previousBlock.type.id;
            save();
        },
        onBreak(event, params) {
            r.onBreak = describe(event, params) + "|" + (event.entitySource !== undefined);
            save();
        },
        onPlayerBreak: log("onPlayerBreak"),
        onPlayerInteract(event, params) {
            r.onPlayerInteract = describe(event, params) + "|" + event.face + "|" + event.faceLocation.x + ","
                + event.faceLocation.z + "|" + (event instanceof BlockComponentPlayerInteractEvent)
                + (event instanceof BlockEvent) + "|" + event.block.x;
            save();
        },
        onTick: log("onTick"),
        onRandomTick: log("onRandomTick"),
        onStepOn: log("onStepOn"),
        onStepOff: log("onStepOff"),
        onEntityFallOn(event, params) {
            r.onEntityFallOn = (r.onEntityFallOn || "") + describe(event, params) + "|" + event.fallDistance + ";";
            save();
        },
        onRedstoneUpdate(event, params) {
            r.onRedstoneUpdate = (r.onRedstoneUpdate || "") + event.previousPowerLevel + ">" + event.powerLevel + ";";
            save();
        },
        onPlayerDestroy: log("onPlayerDestroy"),
    }));
    r.duplicate = capture(() => blocks.registerCustomComponent("test:hooks", {}));
    r.registerItem = capture(() => items.registerCustomComponent("test:wand", {
        onUse: log("onUse"),
        onUseOn(event, params) {
            r.onUseOn = describe(event, params) + "|" + event.blockFace + "|" + event.faceLocation.y + "|"
                + (event instanceof ItemComponentUseOnEvent) + (event instanceof ItemUseOnEvent);
            save();
        },
        onCompleteUse(event, params) {
            r.useOrder = (r.useOrder || "") + event.itemStack.typeId + ":complete;";
            log("onCompleteUse")(event, params);
        },
        onConsume(event, params) {
            r.useOrder = (r.useOrder || "") + event.itemStack.typeId + ":consume;";
            log("onConsume")(event, params);
        },
        onHitEntity(event, params) {
            r.itemOrder = (r.itemOrder || "") + "hit;";
            log("onHitEntity")(event, params);
        },
        onBeforeDurabilityDamage(event, params) {
            r.itemOrder = (r.itemOrder || "") + "durability;";
            r.onBeforeDurabilityDamage = describe(event, params) + "|" + event.durabilityDamage;
            event.durabilityDamage = 7;
            save();
        },
        onMineBlock: log("onMineBlock"),
    }));
    r.itemDuplicate = capture(() => items.registerCustomComponent("test:wand", {}));
    r.litHook = capture(() => blocks.registerCustomComponent("test:lit_hook", {
        onTick(event, params) {
            r.litTick = JSON.stringify(params.params);
            save();
        },
    }));
    r.stateHook = capture(() => blocks.registerCustomComponent("test:states", {
        onBlockStateChange(event, params) {
            r.stateChange = Object.getPrototypeOf(event).constructor.name + "|"
                + event.previousPermutation.getState("test:lit") + "|" + event.previousPermutation.getState("test:level");
            save();
        },
    }));

    const dimensions = event.dimensionRegistry;
    dimensionsLate = dimensions;
    r.dimRegister = capture(() => dimensions.registerCustomDimension("test:void"));
    r.dimDuplicate = capture(() => dimensions.registerCustomDimension("test:void"));
    const reason = (name) => {
        try {
            dimensions.registerCustomDimension(name);
        } catch (e) {
            return e.name + "," + e.reason;
        }
        return "ok";
    };
    r.dimNoNamespace = reason("void");
    r.dimMinecraft = reason("minecraft:void");
    r.dimChars = capture(() => dimensions.registerCustomDimension("test:a b"));

    const clocks = event.worldClockRegistry;
    r.registerClock = capture(() => clocks.registerClock("test:clock",
        { timeMarkers: [{ name: "test:mark", time: 2, period: 5 }] }));
    r.clockDuplicate = capture(() => clocks.registerClock("test:clock"));
    r.clockName = capture(() => clocks.registerClock("noname"));
    r.clockNames = [" test:a", "test:a:b", "test", "minecraft:mine", "test:with space", "Test:Upper"]
        .map((name) => capture(() => clocks.registerClock(name)).startsWith("ok") ? "ok" : "no").join(",");
    r.clockPeriod = capture(() => clocks.registerClock("test:other",
        { timeMarkers: [{ name: "test:bad", time: 5, period: 5 }] }));
    save();
});

world.afterEvents.worldLoad.subscribe(() => {
    r.lateClock = capture(() => clocksLate.registerClock("test:late"));
    r.dimLate = capture(() => dimensionsLate.registerCustomDimension("test:late"));
    const custom = world.getDimension("test:void");
    r.dimId = custom.id;
    r.dimSame = custom === world.getDimension("test:void") && custom instanceof Dimension
        && custom !== world.getDimension("overworld");
    const clock = world.getClock("test:clock");
    r.clockSame = clock === world.getClock("test:clock") && clock instanceof WorldClock;
    r.clockMissing = capture(() => world.getClock("test:nope"));
    r.overworld = world.getClock("minecraft:overworld").timeMarkers
        .map((m) => m.name + "@" + m.time + "/" + m.period).join(",");
    r.resolve = BlockPermutation.resolve("test:stateful", { "test:level": 2 }).getState("test:level") + ","
        + BlockPermutation.resolve("test:stateful").getState("test:lit");
    world.afterEvents.worldClockOnTimeMarker.subscribe((event) => {
        r.markers = (r.markers || "") + event.clock.time + ";";
        save();
    }, { clock: "test:clock" });
    world.afterEvents.worldClockOnTimeMarker.subscribe((event) => {
        r.otherClock = event.clock.name;
    }, { clock: "test:none" });
    world.afterEvents.worldClockOnTimeModified.subscribe((event) => {
        r.modified = (r.modified || 0) + 1;
    }, { clock: "test:clock" });
    world.afterEvents.worldClockOnPaused.subscribe((event) => { r.paused = event.clock.name; }, { clock: "test:clock" });
    world.beforeEvents.worldClockOnRestart.subscribe((event) => {
        r.restart = event.clock.name + "|" + event.newTime;
        event.newTime = 5;
        save();
    }, { clock: "test:clock" });
    save();
});

let clocksLate;
let dimensionsLate;
system.beforeEvents.startup.subscribe((event) => { clocksLate = event.worldClockRegistry; });

system.runTimeout(() => {
    const clock = world.getClock("test:clock");
    r.clockTime = clock.time;
    clock.isPaused = true;
    r.rewind = capture(() => clock.rewindTo("test:mark")) + "@" + clock.time;
    r.skip = capture(() => clock.skipTo(clock.timeMarkers[0])) + "@" + clock.time;
    r.setMarker = capture(() => clock.set("test:mark")) + "@" + clock.time;
    clock.time = 1;
    r.rewindEarly = capture(() => clock.rewindTo("test:mark"));
    r.unknownMarker = capture(() => clock.set("test:unknown"));
    r.addMarker = capture(() => clock.addTimeMarker({ name: "test:added", time: 3 })) + ","
        + capture(() => clock.addTimeMarker({ name: "test:added", time: 4 }));
    const added = clock.timeMarkers.find((m) => m.name === "test:added");
    r.removeMarker = capture(() => clock.removeTimeMarker("test:added")) + "," + capture(() => added.time);
    r.removeMinecraft = capture(() => world.getClock("minecraft:overworld").removeTimeMarker("minecraft:noon"));
    const block = world.getDimension("overworld").getBlock({ x: 4, y: 70, z: 4 });
    r.setPermutation = capture(() => {
        block.setPermutation(BlockPermutation.resolve("test:stateful"));
        block.setPermutation(BlockPermutation.resolve("test:stateful", { "test:lit": true, "test:level": 1 }));
    });
    save();
}, 8);
)JS";

    const char *const COMMANDS = R"JS(
import { world, system, CommandPermissionLevel, CustomCommandParamType, CustomCommandStatus, CustomCommandSource,
         CustomCommandOrigin, CustomCommandRegistry, CustomCommandErrorReason, CommandResult } from "@minecraft/server";
const save = () => world.setDynamicProperty("commands", JSON.stringify(r));
const reason = (f) => {
    try {
        f();
    } catch (e) {
        return e.name + "," + e.reason + "," + e.message;
    }
    return "ok";
};
const describe = (origin, args) => [origin instanceof CustomCommandOrigin, origin.sourceType,
    origin.sourceEntity ? origin.sourceEntity.typeId : "none", origin.sourceBlock ? "block" : "none",
    ...args.map((a) => a === undefined ? "undefined" : Array.isArray(a) ? a.map((e) => e.typeId).join("+")
        : typeof a === "object" ? (a.id || (a.x + "/" + a.y + "/" + a.z)) : String(a))].join(",");
let late;
r.enums = [CustomCommandParamType.PlayerSelector, CustomCommandStatus.Failure, CustomCommandSource.Server,
           CustomCommandErrorReason.EnumDependencyMissing, CommandPermissionLevel.Admin].join(",");
system.beforeEvents.startup.subscribe((event) => {
    const registry = event.customCommandRegistry;
    late = registry;
    r.registry = registry instanceof CustomCommandRegistry;
    r.enumAdd = capture(() => registry.registerEnum("cmd:color", ["red", "blue"]));
    r.enumDuplicate = reason(() => registry.registerEnum("cmd:color", ["x"]));
    r.enumNoNamespace = reason(() => registry.registerEnum("color", []));
    r.enumMismatch = reason(() => registry.registerEnum("other:color", []));
    r.register = capture(() => registry.registerCommand({
        name: "cmd:echo", description: "echo", permissionLevel: CommandPermissionLevel.Any, cheatsRequired: false,
        mandatoryParameters: [{ name: "count", type: CustomCommandParamType.Integer },
                              { name: "cmd:color", type: CustomCommandParamType.Enum }],
        optionalParameters: [{ name: "where", type: CustomCommandParamType.Location },
                             { name: "flag", type: CustomCommandParamType.Boolean },
                             { name: "who", type: CustomCommandParamType.EntitySelector },
                             { name: "block", type: CustomCommandParamType.BlockType }],
    }, (origin, ...args) => {
        r.calls = (r.calls || 0) + 1;
        r.last = describe(origin, args);
        r.restricted = capture(() => world.getDimension("overworld").runCommand("say restricted")) !== "ok";
        system.run(save);
        return args[0] < 0 ? { status: CustomCommandStatus.Failure, message: "negative" }
            : { status: CustomCommandStatus.Success, message: "echo " + args[0] };
    }));
    r.duplicate = reason(() => registry.registerCommand({ name: "cmd:echo", description: "",
        permissionLevel: CommandPermissionLevel.Any }, () => {}));
    r.noNamespace = reason(() => registry.registerCommand({ name: "echo2", description: "",
        permissionLevel: CommandPermissionLevel.Any }, () => {}));
    r.mismatch = reason(() => registry.registerCommand({ name: "other:echo", description: "",
        permissionLevel: CommandPermissionLevel.Any }, () => {}));
    r.missingEnum = reason(() => registry.registerCommand({ name: "cmd:missing", description: "",
        permissionLevel: CommandPermissionLevel.Any,
        mandatoryParameters: [{ name: "cmd:nope", type: CustomCommandParamType.Enum }] }, () => {}));
    r.badType = capture(() => registry.registerCommand({ name: "cmd:bad", description: "",
        permissionLevel: CommandPermissionLevel.Any, mandatoryParameters: [{ name: "x", type: "Nope" }] }, () => {}));
    const nine = [];
    for (let index = 0; index < 9; index++)
        nine.push({ name: "p" + index, type: CustomCommandParamType.Integer });
    r.limit = reason(() => registry.registerCommand({ name: "cmd:nine", description: "",
        permissionLevel: CommandPermissionLevel.Any, mandatoryParameters: nine.slice(0, 5),
        optionalParameters: nine.slice(5) }, () => {}));
    r.eight = capture(() => registry.registerCommand({ name: "cmd:eight", description: "",
        permissionLevel: CommandPermissionLevel.Any, mandatoryParameters: nine.slice(1) }, () => {}));
    r.admin = capture(() => registry.registerCommand({ name: "cmd:admin", description: "",
        permissionLevel: CommandPermissionLevel.Admin, cheatsRequired: false }, () => {}));
    r.give = capture(() => registry.registerCommand({ name: "cmd:give", description: "shadowed",
        permissionLevel: CommandPermissionLevel.GameDirectors }, (origin) => {
        r.secret = describe(origin, []);
        system.run(save);
    }));
    save();
});

world.afterEvents.worldLoad.subscribe(() => {
    r.late = reason(() => late.registerCommand({ name: "cmd:late", description: "",
        permissionLevel: CommandPermissionLevel.Any }, () => {}));
    r.lateEnum = reason(() => late.registerEnum("cmd:late", []));
    const overworld = world.getDimension("overworld");
    const result = overworld.runCommand("cmd:echo 4 red 1 ~2 3");
    r.dimension = (result instanceof CommandResult) + "," + result.successCount + "|" + r.last;
    r.failed = overworld.runCommand("echo -1 blue").successCount;
    r.unknown = capture(() => overworld.runCommand("nosuchcommand"));
    r.parse = capture(() => overworld.runCommand("echo x red"));
    save();
});

system.runTimeout(() => {
    const player = world.getAllPlayers()[0];
    r.playerRun = player.runCommand("echo 6 blue 5 6 7 true @s").successCount + "|" + r.last;
    r.adminDenied = capture(() => player.runCommand("cmd:admin"));
    save();
}, 2);
)JS";

    const char *const BETA_COMMANDS = R"JS(
import { world, system, CommandPermissionLevel, CustomCommandParamType } from "@minecraft/server";
system.beforeEvents.startup.subscribe((event) => {
    const registry = event.customCommandRegistry;
    registry.registerEnum("beta:mode", ["on", "off"]);
    r.named = capture(() => registry.registerCommand({ name: "beta:toggle", description: "",
        permissionLevel: CommandPermissionLevel.Any, cheatsRequired: false,
        mandatoryParameters: [{ name: "state", type: CustomCommandParamType.Enum, enumName: "beta:mode" }] },
        (origin, state) => {
            r.state = state;
            system.run(() => world.setDynamicProperty("betacommands", JSON.stringify(r)));
        }));
    try {
        registry.registerCommand({ name: "beta:wrong", description: "", permissionLevel: CommandPermissionLevel.Any,
            mandatoryParameters: [{ name: "n", type: CustomCommandParamType.Integer, enumName: "beta:mode" }] },
            () => {});
    } catch (e) {
        r.unexpected = e.name + "," + e.reason + "," + e.message;
    }
    world.setDynamicProperty("betacommands", JSON.stringify(r));
});
)JS";

    const char *const COMPONENT_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": { "identifier": "test:block" },
    "components": {
      "minecraft:tick": { "interval_range": [10, 10] },
      "minecraft:redstone_consumer": { "min_power": 3 },
      "minecraft:entity_fall_on": { "min_fall_distance": 2 },
      "minecraft:instrument_sound": { "up": "note.pling", "north": "note.harp" },
      "test:hooks": { "value": 42, "list": ["a", true] }
    }
  }
})JSON";

    const char *const COMPONENT_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:wand" },
    "components": { "test:wand": {} }
  }
})JSON";

    const char *const COMPONENT_FOOD = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:snack" },
    "components": { "minecraft:food": { "nutrition": 2 }, "minecraft:use_duration": 1.6, "test:wand": {} }
  }
})JSON";

    // the slot is looked up lowercased like the game does
    const char *const ENCHANTABLE_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:pick" },
    "components": { "minecraft:enchantable": { "slot": "Pickaxe", "value": 14 } }
  }
})JSON";

    // both forms the game accepts: the value itself, or wrapped in an object
    const char *const WEAPON_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:blade" },
    "components": {
      "minecraft:hand_equipped": true,
      "minecraft:damage": { "value": 6 },
      "minecraft:tags": { "tags": ["minecraft:is_sword", "test:sharp"] }
    }
  }
})JSON";

    const char *const CLUB_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:club" },
    "components": {
      "minecraft:hand_equipped": { "value": true },
      "minecraft:allow_off_hand": true,
      "minecraft:can_destroy_in_creative": { "value": false },
      "minecraft:glint": true,
      "minecraft:liquid_clipped": { "value": true },
      "minecraft:should_despawn": false,
      "minecraft:stacked_by_data": true,
      "minecraft:fire_resistant": { "value": true },
      "minecraft:hover_text_color": "light_purple",
      "minecraft:damage": 3,
      "minecraft:tags": ["test:blunt"]
    }
  }
})JSON";

    // every form the game reads: hex or 0-255 channels (clamped), button text or a bool for the default one
    const char *const CLOAK_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:cloak" },
    "components": {
      "minecraft:dyeable": { "default_color": "#175882" },
      "minecraft:interact_button": "action.test.wave"
    }
  }
})JSON";

    const char *const SASH_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:sash" },
    "components": {
      "minecraft:dyeable": { "default_color": [300, 64, -5] },
      "minecraft:interact_button": true
    }
  }
})JSON";

    const char *const RIBBON_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:ribbon" },
    "components": { "minecraft:dyeable": {}, "minecraft:interact_button": false }
  }
})JSON";

    const char *const STATEFUL_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": {
      "identifier": "test:stateful",
      "states": { "test:lit": [false, true], "test:level": { "values": { "min": 0, "max": 2 } } }
    },
    "components": { "minecraft:tick": { "interval_range": [10, 10] }, "test:states": {} },
    "permutations": [
      { "condition": "q.block_state('test:lit') == true && q.block_state('test:level') >= 1",
        "components": { "test:lit_hook": { "lit": true }, "minecraft:geometry": "geometry.lamp",
                        "minecraft:material_instances": { "*": { "texture": "lit_tex", "render_method": "alpha_test" } } } }
    ]
  }
})JSON";

    // s1 to s4 take 13 of the 16 state bits and s5 would need 4 more; the other states are refused like the game
    // refuses them
    const char *const BOUNDED_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": {
      "identifier": "test:bounded",
      "states": {
        "test:wide": { "values": { "min": 0, "max": 2147483647 } }, "test:one": [1], "test:mixed": [1, "a"],
        "test:same": ["a", "a"], "test:inverted": { "values": { "min": 3, "max": 3 } },
        "test:s1": { "values": { "min": 0, "max": 15 } }, "test:s2": { "values": { "min": 0, "max": 15 } },
        "test:s3": { "values": { "min": 0, "max": 15 } }, "test:s4": [false, true],
        "test:s5": { "values": { "min": 0, "max": 15 } }
      }
    }
  }
})JSON";

    const char *const LEGACY_BLOCK = R"JSON({
  "format_version": "1.21.40",
  "minecraft:block": {
    "description": { "identifier": "legacy:block" },
    "components": { "minecraft:custom_components": ["legacy:comp"] }
  }
})JSON";

    const char *const BETA = R"JS(
import { world, system, Block } from "@minecraft/server";
import { ActionFormData, MessageFormData, ModalFormData, FormResponse, ActionFormResponse, MessageFormResponse,
         ModalFormResponse, FormRejectError, FormRejectReason, FormCancelationReason, UIManager,
         uiManager, CustomForm, MessageBox, ObservableBoolean, ObservableNumber, ObservableString,
         DataDrivenScreenClosedReason } from "@minecraft/server-ui";

r.chatSend = ("chatSend" in world.afterEvents) && ("chatSend" in world.beforeEvents);
r.blockSolid = "isSolid" in Block.prototype;
r.form = typeof ActionFormData === "function"
    && Object.getOwnPropertyNames(new ActionFormData()).length === 0;
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("beta", JSON.stringify(r)));

const ui = {};
const save = () => world.setDynamicProperty("ui", JSON.stringify(ui));
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "ui:forms") return;
    const p = world.getAllPlayers()[0];
    new ActionFormData().title("t").body("b").header("h").label("l").divider().button("one").button("two", "textures/x")
        .show(p).then((res) => {
            ui.action = [res instanceof ActionFormResponse, res instanceof FormResponse, res.canceled, res.selection].join("|");
            save();
        });
    new MessageFormData().body("m").button1("yes").button2("no").show(p).then((res) => {
        ui.message = [res instanceof MessageFormResponse, res.canceled, res.cancelationReason, res.selection].join("|");
        save();
    });
    new ModalFormData().label("l").toggle("t", { defaultValue: true, tooltip: "tip" }).slider("s", 0, 5, { valueStep: 1 })
        .textField("f", "ph").show(p).then((res) => {
            ui.modal = [res instanceof ModalFormResponse, JSON.stringify(res.formValues), res.formValues[0] === undefined].join("|");
            save();
        });
    new ActionFormData().button("x").show(p).catch((e) => {
        ui.malformed = [e instanceof FormRejectError, e.reason, e.message].join("|");
        save();
    });
    uiManager.closeAllForms(p);
    ui.enums = [FormRejectReason.PlayerQuit, FormCancelationReason.UserBusy, uiManager instanceof UIManager].join("|");
    try {
        new ActionFormResponse();
        ui.sealed = "constructed";
    } catch (e) {
        ui.sealed = e.constructor.name;
    }
    save();
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "ui:ddui") return;
    const p = world.getAllPlayers()[0];
    const errorOf = (run) => {
        try {
            run();
            return "none";
        } catch (e) {
            return e.constructor.name + ":" + e.message;
        }
    };
    const toggled = new ObservableBoolean(false, { clientWritable: true });
    const seen = [];
    const listener = toggled.subscribe((value) => seen.push(value));
    ui.readOnly = errorOf(() => new CustomForm(p, "t").toggle("x", new ObservableBoolean(true)));
    const name = new ObservableString("n", { clientWritable: true });
    const form = new CustomForm(p, { rawtext: [{ text: "title" }] }).toggle("on", toggled).button("go", () => {
        ui.clicked = true;
        save();
    }).textField("name", name).slider("s", new ObservableNumber(2, { clientWritable: true }), 0, 5).closeButton();
    ui.notShowing = errorOf(() => form.close());
    form.show().then((reason) => {
        ui.closed = [reason, reason === DataDrivenScreenClosedReason.ClientClosed, form.isShowing(), seen.join(","),
                     toggled.unsubscribe(listener), toggled.unsubscribe(listener)].join("|");
        save();
    });
    ui.showing = form.isShowing();
    ui.modify = errorOf(() => form.label("late"));
    ui.again = errorOf(() => form.show());
    const box = new MessageBox(p, "m", { showHorizontalButtons: true }).body("b").button1("a").button2("b");
    box.show().then((result) => {
        ui.box = [result.closeReason, result.selection].join("|");
        save();
    });
    ui.observable = [new ObservableNumber(3).getData(), errorOf(() => new ObservableNumber("3"))].join("|");
    // 2.x options take an object only, undefined and null meaning none
    ui.primitiveOptions = [errorOf(() => new ModalFormData().toggle("t", true)),
                           errorOf(() => new ModalFormData().slider("s", 0, 5, 1)),
                           errorOf(() => new CustomForm(p, "t").divider(false)),
                           errorOf(() => new ModalFormData().textField("f", "p", null).dropdown("d", ["a"], undefined))]
        .join("|");
    save();
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "ui:filter") return;
    const text = new ObservableString("hello");
    const pending = text.getFilteredText(world.getAllPlayers()[0]);
    // the text is read when asked: a later write does not change the answer
    text.setData("changed");
    ui["filterAsync_" + event.message] = pending instanceof Promise && ui["filter_" + event.message] === undefined;
    pending.then((value) => {
        ui["filter_" + event.message] = JSON.stringify(value);
        save();
    });
    save();
});
)JS";

    // server-ui 1.0.0: only that version's members, and the positional textField default
    const char *const LEGACY_UI = R"JS(
import { world, system } from "@minecraft/server";
import * as ui from "@minecraft/server-ui";
r.positional = typeof new ui.ModalFormData().toggle("t", true).slider("s", 0, 5, 1, 2).dropdown("d", ["a"], 0);
r.members = [typeof ui.ActionFormData.prototype.header, typeof ui.ModalFormData.prototype.submitButton,
             typeof ui.ModalFormData.prototype.label, "uiManager" in ui, typeof ui.ActionFormData.prototype.button]
    .join("|");
const save = () => world.setDynamicProperty("legacyUi", JSON.stringify(r));
world.afterEvents.worldInitialize.subscribe(save);
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "ui:legacy") return;
    new ui.ModalFormData().textField("f", "ph", { rawtext: [{ text: "d" }] }).show(world.getAllPlayers()[0])
        .then((res) => {
            r.response = ["canceled" in res, "cancelationReason" in res, JSON.stringify(res.formValues)].join("|");
            save();
        });
});
)JS";

    const char *const GAPS = R"JS(
import { world, system, MolangVariableMap, TicksPerSecond, TicksPerDay, MoonPhaseCount, FluidContainer, Trigger,
         DimensionTypes, ItemTypes, BlockVolume, BlockVolumeBase, HeldItemOption, PlatformType, GraphicsMode,
         ControlScheme, LiquidSettings, ISerializable, Structure, TextPrimitive, PrimitiveShape } from "@minecraft/server";
r.molangEarly = capture(() => new MolangVariableMap());
r.triggerEarly = capture(() => new Trigger("a:b"));
system.run(() => {
    r.constants = [TicksPerSecond, TicksPerDay, MoonPhaseCount, FluidContainer.minFillLevel, FluidContainer.maxFillLevel]
        .join(",");
    const trigger = new Trigger("minecraft:entity_born");
    trigger.eventName = "test:grow";
    r.trigger = trigger.eventName + "," + (trigger instanceof Trigger);
    r.enums = [HeldItemOption.AnyItem, PlatformType.Console, GraphicsMode.Deferred, ControlScheme.PlayerRelativeStrafe,
               LiquidSettings.IgnoreWaterlogging].join(",");
    r.serializable = Object.getPrototypeOf(Structure.prototype) === ISerializable.prototype;
    const map = new MolangVariableMap();
    map.setFloat("speed", 2);
    map.setColorRGB("variable.tint", { red: 1, green: 0.5, blue: 0 });
    r.molang = map instanceof MolangVariableMap;
    r.molangDot = capture(() => map.setFloat("a.b", 1));
    r.molangType = capture(() => map.setVector3("v", 3));
    r.dimensionTypes = DimensionTypes.getAll().map((type) => type.typeId).join(",");
    r.nether = DimensionTypes.get("minecraft:nether").typeId + "," + DimensionTypes.get("nether");
    r.itemKey = ItemTypes.get("minecraft:apple").localizationKey + "," + ItemTypes.get("minecraft:stone").localizationKey;
    const volume = new BlockVolume({ x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 1 });
    volume.to = { x: 2, y: 0, z: 1 };
    r.volume = [Object.getOwnPropertyNames(volume).length, "getMin" in BlockVolumeBase.prototype,
                Object.getOwnPropertyNames(BlockVolume.prototype).includes("getMin"), volume.getCapacity(),
                volume.to.x].join("|");
    r.music = capture(() => world.playMusic("record.cat", { volume: -1 }));
    r.musicOk = capture(() => world.playMusic("record.cat", { fade: 1, loop: true })) + ","
        + capture(() => world.queueMusic("record.cat")) + "," + capture(() => world.stopMusic());
    r.particle = capture(() => world.getDimension("overworld").spawnParticle("minecraft:basic_flame_particle",
                                                                             { x: 0, y: 100, z: 0 }, map));
    r.particleBadMap = capture(() => world.getDimension("overworld").spawnParticle("minecraft:basic_flame_particle",
                                                                                   { x: 0, y: 100, z: 0 }, {}));
    const manager = world.primitiveShapesManager;
    const label = new TextPrimitive({ x: 1, y: 2, z: 3 }, "hello");
    manager.addText(label);
    manager.addText(label);
    const far = new TextPrimitive({ x: 50, y: 2, z: 3 }, { text: "far" });
    manager.addText(far);
    r.primitives = [manager.maxShapes, label instanceof PrimitiveShape, "hiddenFrom" in label,
                    manager.getShapes().length,
                    manager.getShapes({ location: { x: 0, y: 2, z: 3 }, maxDistance: 10 }).length,
                    manager.getShapes({ location: { x: 0, y: 2, z: 3 }, minDistance: 10 })[0] === far,
                    capture(() => manager.addText({})), label.text, JSON.stringify(far.text)].join("|");
    manager.removeText(label);
    r.primitivesLeft = manager.getShapes().length;
    for (let i = manager.getShapes().length; i < manager.maxShapes; i++)
        manager.addText(new TextPrimitive({ x: 0, y: 0, z: 0 }, "x"));
    r.primitiveLimit = capture(() => manager.addText(new TextPrimitive({ x: 0, y: 0, z: 0 }, "x")));
    manager.removeAll();
    r.primitivesCleared = manager.getShapes().length;
    world.setDynamicProperty("gaps", JSON.stringify(r));
});
)JS";

    const char *const LEGACY = R"JS(
import { world, system, MinecraftDimensionTypes, EntityCanClimbComponent, BlockInventoryComponent,
         EffectAddAfterEvent, ExplosionBeforeEventSignal } from "@minecraft/server";
r.dimensions = [MinecraftDimensionTypes.overworld, MinecraftDimensionTypes.nether, MinecraftDimensionTypes.theEnd]
    .join(",");
r.componentIds = EntityCanClimbComponent.componentId + "," + BlockInventoryComponent.componentId;
r.orphans = typeof EffectAddAfterEvent + "," + typeof ExplosionBeforeEventSignal.prototype.subscribe;
system.run(() => {
    const sound = (options) => { try { world.playSound("random.click", { x: 0, y: 64, z: 0 }, options); return "ok"; }
                                 catch (e) { return "threw"; } };
    r.worldSound = [sound(), sound({ volume: 2, pitch: 0.5 }), sound({ pitch: 0 })].join(",");
    const dimension = world.getDimension("overworld");
    const pending = dimension.runCommandAsync("time set 1000");
    r.immediate = pending instanceof Promise;
    pending.then((result) => {
        r.async = "resolved:" + result.successCount;
        return dimension.runCommandAsync("notacommand");
    }).then(() => { r.asyncError = "resolved"; }, (error) => {
        r.asyncError = error.name;
        world.setDynamicProperty("legacy", JSON.stringify(r));
    });
});
)JS";

    const char *const UNDECLARED = R"JS(
import { world } from "@minecraft/server";
import { ActionFormData } from "@minecraft/server-ui";
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("undeclared", "ran"));
)JS";

    const char *const NEVER_RUNS = R"JS(
import { world } from "@minecraft/server";
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("rejected", "ran"));
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner, const std::string &key) {
        const auto it = owner.getWorldDynamicProperties().find(key);
        require(it != owner.getWorldDynamicProperties().end(), "no result from the " + key + " pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the " + key + " pack wrote no JSON");
        return parsed;
    }

    std::string text(const json::Value &values, const char *key) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        if (value->mType == json::Value::Type::Boolean)
            return value->mBoolean ? "true" : "false";
        if (value->isNumber())
            return std::to_string(value->integer(0));
        return value->string();
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const std::string actual = text(values, key);
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    DimensionDefinition sentDimension;

    bool fromBlockCheats(Level &level);

    // a command block's origin, as CommandBlockSystem builds it
    class BlockOrigin final : public CommandOrigin {
    public:
        BlockOrigin(Level &level, const Vector3i &position) : mLevel(level), mPosition(position) {}

        std::string getSenderName() const override {
            return "@";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &) override {}

        void sendTranslation(const std::string &, const std::vector<std::string> &) override {}

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }

        bool getSourceBlock(Level *&level, Vector3i &position) override {
            level = &mLevel;
            position = mPosition;
            return true;
        }

    private:
        Level &mLevel;
        Vector3i mPosition;
    };

    bool fromBlockCheats(Level &level) {
        BlockOrigin origin(level, Vector3i(0, 0, 0));
        return origin.canUseCommandsWithoutCheats();
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        const std::string server2 = "{\"module_name\": \"@minecraft/server\", \"version\": \"2.11.0\"}";
        writePack(packs, "stable2", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  std::string(PRELUDE) + STABLE2);
        writePack(packs, "stable1", "{\"module_name\": \"@minecraft/server\", \"version\": \"1.19.0\"}",
                  std::string(PRELUDE) + STABLE1);
        writePack(packs, "beta",
                  "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"},"
                  " {\"module_name\": \"@minecraft/server-ui\", \"version\": \"2.4.0-beta\"}",
                  std::string(PRELUDE) + BETA);
        writePack(packs, "legacyui",
                  "{\"module_name\": \"@minecraft/server\", \"version\": \"1.19.0\"},"
                  " {\"module_name\": \"@minecraft/server-ui\", \"version\": \"1.0.0\"}",
                  std::string(PRELUDE) + LEGACY_UI);
        writePack(
                packs, "world", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                std::string(PRELUDE) + WORLD,
                ", \"settings\": [{\"type\": \"label\", \"text\": \"x\"},"
                " {\"type\": \"toggle\", \"name\": \"test:on\", \"default\": true},"
                " {\"type\": \"slider\", \"name\": \"test:level\", \"default\": 4, \"min\": 0, \"max\": 8, \"step\": 1}]");
        writePack(packs, "entities", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  std::string(PRELUDE) + ENTITIES);
        writePack(packs, "betaentities",
                  "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"},"
                  " {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  std::string(PRELUDE) + BETA_ENTITIES);
        writePack(packs, "items", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  std::string(PRELUDE) + ITEMS);
        writePack(packs, "graphics",
                  server2 + ", {\"module_name\": \"@minecraft/server-graphics\", \"version\": \"1.0.0\"},"
                            " {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  GRAPHICS);
        writePack(packs, "loot", server2, LOOT);
        writePack(packs, "sounds", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  SOUNDS);
        writeFile(packs / "sounds" / "sounds" / "sound_definitions.json",
                  R"({"format_version": "beta", "minecraft:server_sound_definitions": [
                      {"sound_event_name": "test:jingle", "duration_info": {"mode": "game_time", "seconds": 0.1},
                       "music_info": {"artist": "Someone", "genres": ["Pop", "Folk"], "title": "Tune"},
                       "tags": {"mood": "calm"}},
                      {"sound_event_name": "random.orb"},
                      {"sound_event_name": "test:ticks", "duration_info": {"mode": "game_time", "seconds": 0.125}},
                      {"sound_event_name": "test:clock", "duration_info": {"mode": "real_time", "seconds": 1.2345}},
                      {"sound_event_name": "test:ticks", "duration_info": {"mode": "game_time", "seconds": 9}},
                      {"sound_event_name": "test:ranged", "broadcast_range": 4}]})");
        const std::string gemTable = "{\"pools\": [{\"rolls\": 1, \"entries\": [{\"type\": \"item\", "
                                     "\"name\": \"minecraft:diamond\", \"functions\": [{\"function\": "
                                     "\"set_count\", \"count\": 3}]}]}]}";
        writeFile(packs / "loot" / "loot_tables" / "custom" / "gem.json", gemTable);
        writeFile(packs / "loot" / "loot_tables" / "chests" / "igloo_chest.json",
                  "{\"pools\": [{\"rolls\": 1, \"entries\": [{\"type\": \"item\", "
                  "\"name\": \"minecraft:emerald\"}]}]}");
        writePack(packs, "gaps", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  std::string(PRELUDE) + GAPS);
        writePack(packs, "legacy", "{\"module_name\": \"@minecraft/server\", \"version\": \"1.8.0\"}",
                  std::string(PRELUDE) + LEGACY);
        writePack(packs, "undeclared", server2, UNDECLARED);
        writePack(packs, "badversion", "{\"module_name\": \"@minecraft/server\", \"version\": \"2.99.0\"}", NEVER_RUNS);
        writePack(packs, "unknown",
                  server2 + ", {\"module_name\": \"@minecraft/server-unknown\", \"version\": \"1.0.0\"}", NEVER_RUNS);
        writeFile(packs / "outside.js", "export const x = 1;\n");
        writePack(packs, "components", server2, std::string(PRELUDE) + COMPONENTS);
        writePack(packs, "commands", server2, std::string(PRELUDE) + COMMANDS);
        writePack(packs, "betacommands", "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}",
                  std::string(PRELUDE) + BETA_COMMANDS);
        writeFile(packs / "components" / "blocks" / "block.json", COMPONENT_BLOCK);
        writeFile(packs / "components" / "items" / "wand.json", COMPONENT_ITEM);
        writeFile(packs / "components" / "items" / "snack.json", COMPONENT_FOOD);
        writeFile(packs / "components" / "items" / "pick.json", ENCHANTABLE_ITEM);
        writeFile(packs / "components" / "items" / "blade.json", WEAPON_ITEM);
        writeFile(packs / "components" / "items" / "club.json", CLUB_ITEM);
        writeFile(packs / "components" / "items" / "cloak.json", CLOAK_ITEM);
        writeFile(packs / "components" / "items" / "sash.json", SASH_ITEM);
        writeFile(packs / "components" / "items" / "ribbon.json", RIBBON_ITEM);
        writeFile(packs / "components" / "blocks" / "stateful.json", STATEFUL_BLOCK);
        writeFile(packs / "components" / "blocks" / "bounded.json", BOUNDED_BLOCK);
        writeFile(packs / "stable1" / "blocks" / "legacy.json", LEGACY_BLOCK);

        ServerNetworkHandler owner("Script API", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);

        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        const CustomBlockDefinition *bounded = CustomContentRegistry::getInstance().findBlock("test:bounded");
        require(bounded != nullptr && bounded->mStates.size() == 4 && bounded->mStates.front().mName == "test:s1"
                        && bounded->mStates.back().mName == "test:s4"
                        && CustomContentRegistry::permutationsOf(*bounded).size() == 8192,
                "the pack block states were not bounded like the game");
        for (const BlockPropertyData &property: CustomContentRegistry::getInstance().getBlockProperties()) {
            if (property.mName != "test:stateful")
                continue;
            const Tag *permutations = property.mProperties.get("permutations");
            require(permutations != nullptr && permutations->getList().size() == 1, "permutations not sent");
            const Tag &first = permutations->getList().front();
            const Tag *components = first.get("components");
            require(components != nullptr && components->get("test:lit_hook") == nullptr,
                    "a custom component reached the client");
            const Tag *geometry = components->get("minecraft:geometry");
            const Tag *materials = components->get("minecraft:material_instances");
            require(geometry != nullptr && geometry->get("identifier")->asString() == "geometry.lamp",
                    "the permutation geometry was not sent");
            require(materials != nullptr
                            && materials->get("materials")->get("*")->get("texture")->asString() == "lit_tex"
                            && materials->get("materials")->get("*")->get("render_method")->asString() == "alpha_test",
                    "the permutation material was not sent");
            require(components->get("minecraft:friction") == nullptr, "an undeclared component was sent");
            const Tag *base = property.mProperties.get("components")->get("minecraft:material_instances");
            require(base->get("materials")->get("*")->get("texture")->asString() == "stateful",
                    "the permutation changed the base material");
            const Tag *states = property.mProperties.get("properties");
            require(states != nullptr && states->getList().size() == 2, "states not sent");
        }
        {
            const std::shared_ptr<ItemDefinition> pick = owner.getItemDefinitions().getDefinition("test:pick");
            require(pick != nullptr, "the enchantable item was not registered");
            const Tag *components = pick->getComponentData().get("components");
            const Tag *properties = components == nullptr ? nullptr : components->get("item_properties");
            require(properties != nullptr && properties->getString("enchantable_slot") == "pickaxe"
                            && properties->getInt("enchantable_value") == 14,
                    "the enchantable item properties were not sent");
            const Tag *enchantable = components->get("minecraft:enchantable");
            require(enchantable != nullptr && enchantable->getString("slot") == "pickaxe"
                            && enchantable->getByte("value") == 14,
                    "the enchantable component was not sent");
            require(ItemEnchantments::getItemTargets("test:pick") == EnchantmentTargets::PICKAXE
                            && EnchantmentHelper::getEnchantability("test:pick") == 14,
                    "the enchantable item has no enchantment slot on the server");
            const EnchantmentData &efficiency = *EnchantmentTable::findById(EnchantmentIds::EFFICIENCY);
            const EnchantmentData &sharpness = *EnchantmentTable::findById(EnchantmentIds::SHARPNESS);
            require(!EnchantmentHelper::enchantWithLevels("test:pick", 30, false, 1).empty()
                            && ItemEnchantments::canApply("test:pick", efficiency)
                            && !ItemEnchantments::canApply("test:pick", sharpness),
                    "the enchanting table offers the wrong enchantments for the enchantable item");
            require(ItemEnchantments::getItemTargets("test:wand") == EnchantmentTargets::NONE,
                    "an item without minecraft:enchantable became enchantable");
        }
        {
            const std::shared_ptr<ItemDefinition> blade = owner.getItemDefinitions().getDefinition("test:blade");
            require(blade != nullptr, "the weapon item was not registered");
            const Tag *components = blade->getComponentData().get("components");
            const Tag *properties = components == nullptr ? nullptr : components->get("item_properties");
            require(properties != nullptr && properties->getByte("hand_equipped") == 1
                            && properties->getInt("damage") == 6,
                    "the weapon item properties were not sent");
            const Tag *handEquipped = components->get("minecraft:hand_equipped");
            const Tag *damage = components->get("minecraft:damage");
            require(handEquipped != nullptr && handEquipped->getByte("value") == 1 && damage != nullptr
                            && damage->get("value") != nullptr && damage->get("value")->asShort() == 6,
                    "the weapon components were not sent");
            const Tag *itemTags = components->get("item_tags");
            const Tag *tagsComponent = components->get("minecraft:tags");
            require(itemTags != nullptr && itemTags->getList().size() == 2
                            && itemTags->getList().front().asString() == "minecraft:is_sword"
                            && tagsComponent != nullptr && tagsComponent->get("tags") != nullptr
                            && tagsComponent->get("tags")->getList().size() == 2,
                    "the weapon tags were not sent");

            const std::vector<std::string> &bladeTags = CraftingRecipeTable::getItemTags("test:blade");
            require(bladeTags == std::vector<std::string>{"minecraft:is_sword", "test:sharp"}
                            && CraftingRecipeTable::getItemTags("test:club") == std::vector<std::string>{"test:blunt"},
                    "the server does not know the pack item tags");
            Item bladeItem;
            Item clubItem;
            require(StringToItemParser::getInstance().parse("test:blade", bladeItem) && bladeItem.getAttackDamage() == 6
                            && StringToItemParser::getInstance().parse("test:club", clubItem)
                            && clubItem.getAttackDamage() == 3,
                    "the pack item attack damage did not reach the server item");

            const std::shared_ptr<ItemDefinition> club = owner.getItemDefinitions().getDefinition("test:club");
            const Tag *clubProperties = club->getComponentData().get("components")->get("item_properties");
            require(clubProperties->getByte("hand_equipped") == 1 && clubProperties->getInt("damage") == 3,
                    "the short forms were not read");
            const Tag *clubComponents = club->getComponentData().get("components");
            require(clubProperties->getByte("allow_off_hand") == 1
                            && clubProperties->getByte("can_destroy_in_creative") == 0
                            && clubProperties->getByte("foil") == 1 && clubProperties->getByte("liquid_clipped") == 1
                            && clubProperties->getByte("should_despawn") == 0
                            && clubProperties->getByte("stacked_by_data") == 1
                            && clubComponents->get("minecraft:fire_resistant") != nullptr
                            && clubComponents->get("minecraft:fire_resistant")->getByte("value") == 1,
                    "the pack item flags were not sent");
            const Tag *wandProperties =
                    owner.getItemDefinitions().getDefinition("test:wand")->getComponentData().get("components")->get(
                            "item_properties");
            require(wandProperties->getByte("hidden_in_commands") == 2
                            && wandProperties->get("is_hidden_in_commands") == nullptr,
                    "the command visibility is not under the game's key");
            require(wandProperties->getByte("allow_off_hand") == 0
                            && wandProperties->getByte("can_destroy_in_creative") == 1
                            && wandProperties->getByte("foil") == 0 && wandProperties->getByte("liquid_clipped") == 0
                            && wandProperties->getByte("should_despawn") == 1
                            && wandProperties->getByte("stacked_by_data") == 0,
                    "an item without the flags lost the game's defaults");
            require(clubProperties->getString("hover_text_color") == "light_purple"
                            && wandProperties->get("hover_text_color") == nullptr,
                    "the hover text color is sent wrong");
            const Tag *wandComponents =
                    owner.getItemDefinitions().getDefinition("test:wand")->getComponentData().get("components");
            require(wandComponents->get("item_tags") != nullptr && wandComponents->get("item_tags")->getList().empty()
                            && wandComponents->get("minecraft:tags") == nullptr
                            && wandComponents->get("minecraft:hand_equipped") == nullptr
                            && wandComponents->get("minecraft:damage") == nullptr,
                    "an item without the components got them");
        }
        {
            const auto componentsOf = [&owner](const char *identifier) {
                const std::shared_ptr<ItemDefinition> definition = owner.getItemDefinitions().getDefinition(identifier);
                require(definition != nullptr, std::string(identifier) + " was not registered");
                return definition->getComponentData().get("components");
            };
            const auto defaultColor = [](const Tag *components) {
                const Tag *dyeable = components->get("minecraft:dyeable");
                const Tag *color = dyeable == nullptr ? nullptr : dyeable->get("default_color");
                std::string channels;
                if (color == nullptr)
                    return channels;
                for (const Tag &channel: color->getList())
                    channels += channel.getType() == Tag::Type::Int ? std::to_string(channel.asInt()) + "," : "?";
                return channels;
            };
            const Tag *cloak = componentsOf("test:cloak");
            const Tag *sash = componentsOf("test:sash");
            const Tag *ribbon = componentsOf("test:ribbon");
            require(defaultColor(cloak) == "23,88,130," && defaultColor(sash) == "255,64,0,"
                            && defaultColor(ribbon) == "255,255,255,",
                    "the dyeable default colors were not sent as the game writes them");
            const auto buttonText = [](const Tag *components) {
                const Tag *button = components->get("minecraft:interact_button");
                return button != nullptr && button->getType() == Tag::Type::String ? button->asString() : std::string();
            };
            require(buttonText(cloak) == "action.test.wave" && buttonText(sash) == "action.interact.use"
                            && buttonText(ribbon) == "action.interact.use",
                    "the interact button text was not sent");
            const Tag *wandComponents =
                    owner.getItemDefinitions().getDefinition("test:wand")->getComponentData().get("components");
            require(wandComponents->get("minecraft:dyeable") == nullptr
                            && wandComponents->get("minecraft:interact_button") == nullptr,
                    "an item without dyeable or interact_button got them");
        }
                            && wandComponents->get("minecraft:fire_resistant") == nullptr
                            && wandComponents->get("minecraft:damage") == nullptr,
                    "an item without the components got them");
        }
        require(engine.loadPacks(manager.getPacks()) == 16, "only the sixteen valid packs start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        engine.startup();
        EntityDefinitions::initialize();
        World &world = owner.getWorlds().getDefault();
        world.getOverworld().insertChunk(LevelChunk(0, 0));
        world.getOverworld().setBlock(Vector3i(0, 101, 5),
                                      VanillaBlocks::fromIdentifier("minecraft:oak_fence")->toBlockState(), false);
        world.getOverworld().setBlock(Vector3i(0, 101, 8),
                                      VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState(), false);
        player.setName("ScriptPlayer");
        player.getClientDevice().mMemoryTier = 2;
        player.getClientDevice().mMaxViewDistance = 96;
        player.getClientDevice().mPlatformType = 1;
        player.getClientDevice().mGraphicsMode = 2;
        player.getInputState().mTouchOnlyHotbar = true;
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(world.getOverworld());
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        ServerActor *cow = owner.spawnActor(world.getOverworld(), "minecraft:cow", Vector3f(0.5f, 100.0f, 2.5f));
        require(cow != nullptr, "no fixture cow");
        if (MobActor *mob = dynamic_cast<MobActor *>(cow))
            mob->startDefinition(owner);
        owner.setSpawnCause("Born");
        ServerActor *calf = owner.spawnActor(world.getOverworld(), "minecraft:cow", Vector3f(4.5f, 100.0f, 4.5f));
        require(calf != nullptr, "no fixture calf");
        ServerActor *wolf = owner.spawnActor(world.getOverworld(), "minecraft:wolf", Vector3f(8.5f, 100.0f, 8.5f));
        MobActor *wolfMob = dynamic_cast<MobActor *>(wolf);
        require(wolfMob != nullptr, "no fixture wolf");
        wolfMob->startDefinition(owner);
        require(wolfMob->tameTo(owner, player, "minecraft:tameable", true), "the wolf was not tamed");
        ServerActor *horse = owner.spawnActor(world.getOverworld(), "minecraft:horse", Vector3f(-8.5f, 100.0f, 8.5f));
        require(dynamic_cast<MobActor *>(horse) != nullptr, "no fixture horse");
        static_cast<MobActor *>(horse)->startDefinition(owner);
        ServerActor *piglin = owner.spawnActor(world.getOverworld(), "minecraft:piglin",
                                               Vector3f(14.5f, 100.0f, 14.5f));
        MobActor *piglinMob = dynamic_cast<MobActor *>(piglin);
        require(piglinMob != nullptr, "no fixture piglin");
        piglinMob->startDefinition(owner);
        ServerActor *minecart = owner.spawnActor(world.getOverworld(), "minecraft:chest_minecart",
                                                 Vector3f(-14.5f, 100.0f, -14.5f));
        require(minecart != nullptr && minecart->getEntityContainer() != nullptr, "no fixture chest minecart");
        {
            // a pack item with minecraft:dyeable takes the water color and washes off like leather armor
            Level &level = world.getOverworld();
            const Vector3i dyed(12, 110, 12);
            const Vector3i plain(13, 110, 12);
            level.setBlock(dyed, CauldronBlock::makeState(CauldronBlock::Liquid::Water, 3), false);
            level.setBlock(plain, CauldronBlock::makeState(CauldronBlock::Liquid::Water, 3), false);
            CauldronBlockActor &cauldron = level.getBlockActors().getOrCreate<CauldronBlockActor>(dyed);
            cauldron.addDye(CauldronBlockActor::dyeColorOf("minecraft:red_dye"));
            const int32_t color = (int32_t) (0xFF000000u | ((uint32_t) cauldron.getColor() & 0xFFFFFFu));
            const auto use = [&](const Vector3i &at, const char *identifier) {
                player.getInventory().setItemInHand(owner.createItemStack(identifier, 1));
                const BlockState state = level.getBlockState(at.x, at.y, at.z);
                return VanillaBlocks::fromIdentifier(state.mName)->onInteract(owner, player, at, state);
            };
            const ItemStack previous = player.getInventory().getItemInHand();
            require(use(dyed, "test:cloak")
                            && player.getInventory().getItemInHand().mTag.getInt("customColor", 0) == color
                            && CauldronBlock(level.getBlockState(dyed.x, dyed.y, dyed.z)).getFillLevel() == 2,
                    "the cauldron did not dye the dyeable pack item");
            ItemStack worn = player.getInventory().getItemInHand();
            require(!use(dyed, "test:wand"), "the cauldron dyed a pack item without minecraft:dyeable");
            player.getInventory().setItemInHand(worn);
            const BlockState water = level.getBlockState(plain.x, plain.y, plain.z);
            require(VanillaBlocks::fromIdentifier(water.mName)->onInteract(owner, player, plain, water)
                            && !player.getInventory().getItemInHand().mTag.contains("customColor"),
                    "plain water did not wash the dyeable pack item");
            player.getInventory().setItemInHand(previous);
            level.setBlock(dyed, BlockState(), false);
            level.setBlock(plain, BlockState(), false);
            level.getBlockActors().remove(dyed);
            level.getBlockActors().remove(plain);
        }
        player.setHealth(10.0f);
        engine.tick(0);
        player.heal(5.0f, HealCause::Regeneration);
        owner.hurt(player, 1.0f, ActorDamageSource::environment("death.attack.magic", player.getName()));
        engine.onPlayerEmote(player, "emote:wave");
        const int64_t loadedId = player.getUniqueId() + 77;
        player.setUniqueId(loadedId);
        engine.onPlayerButtonInput(player, "Jump", true);
        engine.onPlayerInputModeChange(player, "KeyboardAndMouse", "Touch");
        engine.onPlayerSwingStart(player, "Attack");
        engine.onEntitySneak(player, true);
        engine.onEntitySneak(player, false);
        engine.onPlayerInteractWithEntity(player, *cow, player.getInventory().getItemInHand());
        engine.onPlayerDimensionChange(player, world.getOverworld(), player.getPosition(),
                                       world.getLevel(DimensionType::Nether), player.getPosition());
        MobEffectInstance speed;
        speed.mId = MobEffectId::Speed;
        speed.mDuration = 400;
        player.addEffect(speed);

        bool customFound = false;
        const DimensionType voidDimension = Dimension::findCustom("test:void", customFound);
        require(customFound && Dimension::toId(voidDimension) == Dimension::FIRST_CUSTOM_ID
                        && Dimension::fromId(Dimension::FIRST_CUSTOM_ID) == voidDimension
                        && std::string(Dimension::getName(voidDimension)) == "test:void",
                "the custom dimension did not get the first custom id");
        World &defaultWorld = owner.getWorlds().getDefault();

        // a packet sensor: every encoded packet, dropped before any transport
        require(owner.isTransportReady(), "the test server has no network handler to observe");
        std::vector<std::pair<MinecraftPacketIds, std::string>> sent;
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });
        LoginHandler::sendStartGame(owner, player);
        size_t startGameAt = sent.size(), dimensionsAt = sent.size();
        for (size_t index = 0; index < sent.size(); ++index) {
            if (sent[index].first == MinecraftPacketIds::StartGame && startGameAt == sent.size())
                startGameAt = index;
            if (sent[index].first == MinecraftPacketIds::DimensionData && dimensionsAt == sent.size())
                dimensionsAt = index;
        }
        require(startGameAt < sent.size() && dimensionsAt == startGameAt + 1,
                "DimensionDataPacket does not follow StartGame");
        ReadOnlyBinaryStream body(sent[dimensionsAt].second);
        DimensionDataPacket dimensionData;
        dimensionData.readHeader(body);
        dimensionData.read(body, owner.getCodecContext());
        require(dimensionData.mDefinitions.size() == 1 && dimensionData.mDefinitions[0].mId == "test:void",
                "DimensionDataPacket does not carry the custom dimension");
        sentDimension = dimensionData.mDefinitions[0];
        require(sentDimension.mMinimumHeight == -64 && sentDimension.mMaximumHeight == 384
                        && sentDimension.mGeneratorType == 5 && sentDimension.mCloudHeight == 128
                        && !sentDimension.mRenderClouds,
                "DimensionDataPacket does not carry the game's script dimension definition");

        sent.clear();
        owner.changePlayerLevel(player, defaultWorld.getLevel(voidDimension), Vector3f(0.5f, 80.0f, 0.5f));
        require(player.getDimension() == voidDimension, "the player did not move to the custom dimension");
        bool changed = false;
        for (const auto &packet: sent) {
            if (packet.first != MinecraftPacketIds::ChangeDimension)
                continue;
            ReadOnlyBinaryStream change(packet.second);
            ChangeDimensionPacket decoded;
            decoded.readHeader(change);
            decoded.read(change, owner.getCodecContext());
            changed = decoded.mDimension == Dimension::FIRST_CUSTOM_ID;
        }
        require(changed, "no ChangeDimensionPacket named the custom dimension");
        owner.changePlayerLevel(player, defaultWorld.getOverworld(), Vector3f(0.5f, 80.0f, 0.5f));
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        // back where the entities pack expects a spawned player
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        require(defaultWorld.hasLevel(voidDimension)
                        && defaultWorld.getLevel(voidDimension).getDimensionType() == voidDimension
                        && &defaultWorld.getLevel(voidDimension) != &defaultWorld.getOverworld(),
                "no level was created for the custom dimension");
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(voidDimension, 1);
        const std::unique_ptr<LevelChunk> terrain = generator->generateTerrain(0, 0);
        require(terrain->getBlock(0, 64, 0).mName == "minecraft:air"
                        && terrain->getBlock(0, LevelChunk::MIN_Y, 0).mName == "minecraft:air",
                "the custom dimension is not generated empty");

        Level &level = owner.getLevelFor(player);
        const Vector3i at(4, 70, 4);
        BlockState placed("test:block");
        require(engine.beforeCustomBlockPlace(player, level, at, 1, placed), "beforeOnPlayerPlace did not cancel");
        require(!engine.beforeCustomBlockPlace(player, level, at, 1, placed) && placed.mName == "minecraft:stone",
                "beforeOnPlayerPlace did not replace the permutation");
        engine.onCustomBlockPlaced(level, at, BlockState("test:block"), BlockState("minecraft:air"));
        engine.onPlayerInteractWithBlock(player, "test:block", at, 1, Vector3f(0.25f, 1.0f, 0.75f));
        engine.onPlayerInteractWithBlock(player, "legacy:block", at, 1, Vector3f(0.25f, 1.0f, 0.75f));
        engine.onCustomBlockTick(level, "test:block", at);
        engine.onCustomBlockRandomTick(level, "test:block", at);
        engine.onCustomBlockStep(player, level, "test:block", at);
        engine.onCustomBlockStepOff(player, level);
        engine.onCustomBlockFallOn(player, level, "test:block", at, 1.5f);
        engine.onCustomBlockFallOn(player, level, "test:block", at, 3.5f);
        for (int32_t power: {0, 2, 5, 5, 9, 0})
            engine.onCustomBlockRedstone(level, "test:block", at, power);
        // a block placed again starts from no power: the same 9 fires once more
        engine.onCustomBlockRedstone(level, "test:block", at, 9);
        engine.onCustomBlockPlaced(level, at, BlockState("test:block"), BlockState("minecraft:air"));
        engine.onCustomBlockRedstone(level, "test:block", at, 9);
        engine.onCustomBlockBreak(level, at, BlockState("test:block"), &player);
        PlayerBreakBlockAfterEvent legacyBroken(player, at, "legacy:block");
        owner.getEventBus().after().mPlayerBreakBlock.emit(legacyBroken);

        ItemStack wand;
        wand.mDefinition = owner.getItemDefinitions().getDefinition("test:wand");
        wand.mCount = 1;
        require(wand.mDefinition != nullptr, "the custom item was not registered");
        player.getInventory().setItemInHand(wand);
        engine.onItemUse(player);
        engine.onItemUseOnBlock(player, at, 1, Vector3f(0.5f, 1.0f, 0.5f));
        engine.onItemConsume(player, wand);
        engine.onItemCompleteUse(player, wand);
        ItemStack snack;
        snack.mDefinition = owner.getItemDefinitions().getDefinition("test:snack");
        snack.mCount = 1;
        require(snack.mDefinition != nullptr, "the custom food was not registered");
        engine.onItemConsume(player, snack);
        engine.onItemCompleteUse(player, snack);
        require(engine.onItemHitEntity(player, player, true, 2) == 7, "onBeforeDurabilityDamage was not applied");
        PlayerBreakBlockAfterEvent broken(player, at, "test:block");
        broken.mItemBeforeBreak = wand;
        owner.getEventBus().after().mPlayerBreakBlock.emit(broken);
        player.getInventory().setItemInHand(ItemStack::air());

        MobEffectInstance effect;
        effect.mId = MobEffectId::Speed;
        effect.mDuration = 20;
        engine.beforeEffectAdd(player, effect);

        owner.changeGameRule(world, "keepinventory", "true");
        owner.changeGameRule(world, "randomtickspeed", "3");
        owner.setRaining(world, true);
        require(!world.getOverworld().isRaining(), "a cancelled weatherChange still changed the weather");
        owner.setRaining(world, true);
        require(world.getOverworld().isThundering() && world.getOverworld().getRainTime() == 1234,
                "weatherChange did not apply the new weather and duration");

        ScriptEventCommand scriptEvent(owner);
        ServerCommandOrigin console(&owner);
        scriptEvent.execute(console, {"test:ping", "hello", "world"});
        PlayerCommandOrigin playerOrigin(owner, player, CommandOriginData());
        scriptEvent.execute(playerOrigin, {"other:pong"});
        require(!scriptEvent.execute(console, {"minecraft:nope", "x"}), "a minecraft namespace was accepted");
        require(!scriptEvent.execute(console, {"nonamespace"}), "an id without namespace was accepted");
        ItemStack apple = owner.createItemStack("minecraft:apple", 1);
        engine.onItemStartUse(player, apple, 32);
        engine.onItemStopUse(player, owner.createItemStack("minecraft:bow", 1), 5, true);
        engine.onItemCompleteUse(player, apple, 0);
        ItemStack diamonds = owner.createItemStack("minecraft:diamond", 2);
        ItemActor *dropped = ItemActorHandler::dropItem(owner, world.getOverworld(), Vector3f(0.5f, 100.0f, 1.5f),
                                                        diamonds, Vector3f(0.0f, 0.0f, 0.0f), 0);
        require(dropped != nullptr, "no fixture item entity");
        require(!engine.beforeEntityItemPickup(player, *dropped, diamonds), "a player pickup was cancelled");
        engine.onEntityItemPickup(player, diamonds);
        require(engine.beforeEntityItemPickup(*piglin, *dropped, diamonds), "entityItemPickup did not cancel");
        player.getInventory().setCursor(owner.createItemStack("minecraft:diamond", 1));
        // the items pack clears it at tick 2: a grab then a release
        for (int64_t tick = 1; tick <= 3; ++tick)
            engine.tick(tick);

        // forms: the requests carry the 26.60 JSON, the answers come back as typed responses
        sent.clear();
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });
        scriptEvent.execute(console, {"ui:forms"});
        engine.tick(3);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        std::vector<ModalFormRequestPacket> requests;
        bool closed = false;
        for (const auto &packet: sent) {
            closed = closed || packet.first == MinecraftPacketIds::ClientboundCloseForm;
            if (packet.first != MinecraftPacketIds::ModalFormRequest)
                continue;
            ReadOnlyBinaryStream body(packet.second);
            ModalFormRequestPacket request;
            request.readHeader(body);
            request.read(body, owner.getCodecContext());
            requests.push_back(request);
        }
        require(requests.size() == 4, "the forms were not all sent");
        require(closed, "closeAllForms sent no ClientboundCloseFormPacket");
        const std::unique_ptr<json::Value> action = json::parse(requests[0].mFormData);
        const json::Value *elements = action->get("elements");
        require(action->get("buttons") == nullptr && elements != nullptr && elements->mArray.size() == 5,
                "the action form does not list its elements");
        const char *const kinds[] = {"header", "label", "divider", "button", "button"};
        for (size_t index = 0; index < 5; ++index)
            require(elements->mArray[index]->get("type")->string() == kinds[index],
                    "an action form element has the wrong type");
        require(elements->mArray[4]->get("image")->get("data")->string() == "textures/x", "the button icon was lost");
        const std::unique_ptr<json::Value> modal = json::parse(requests[2].mFormData);
        require(modal->get("content")->mArray[1]->get("tooltip")->string() == "tip", "the toggle tooltip was not sent");

        const auto answer = [&](const ModalFormRequestPacket &request, const char *data, bool busy) {
            ModalFormResponsePacket response;
            response.mFormId = request.mFormId;
            response.mHasFormData = data != nullptr;
            response.mFormData = data != nullptr ? data : "";
            response.mHasCancelReason = data == nullptr;
            response.mCancelReason = busy ? ModalFormResponsePacket::CancelReason::UserBusy
                                          : ModalFormResponsePacket::CancelReason::UserClosed;
            BinaryStream stream;
            response.writeWithHeader(stream, owner.getCodecContext());
            static_cast<NetworkHandler::Listener &>(owner).onDataReceived(id, stream.getBuffer());
        };
        answer(requests[0], "1", false);
        answer(requests[1], nullptr, true);
        answer(requests[2], "[null,true,3,\"hi\"]", false);
        answer(requests[3], "\"x\"", false);
        engine.tick(3);
        const std::unique_ptr<json::Value> forms = result(owner, "ui");
        expect(*forms, "action", "true|true|false|1");
        expect(*forms, "message", "true|true|UserBusy|");
        expect(*forms, "modal", "true|[null,true,3,\"hi\"]|true");
        expect(*forms, "malformed", "true|MalformedResponse|Response from the client was malformed.");
        expect(*forms, "enums", "PlayerQuit|UserBusy|true");
        expect(*forms, "sealed", "ReferenceError");

        // DDUI: the store property and the screen go out together; client writes come back through the counts
        sent.clear();
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });
        scriptEvent.execute(console, {"ui:ddui"});
        engine.tick(3);
        std::vector<ClientboundDataStorePacket> stores;
        std::vector<ClientboundDataDrivenUIShowScreenPacket> screens;
        for (const auto &packet: sent) {
            ReadOnlyBinaryStream body(packet.second);
            if (packet.first == MinecraftPacketIds::ClientboundDataStore) {
                ClientboundDataStorePacket store;
                store.readHeader(body);
                store.read(body, owner.getCodecContext());
                stores.push_back(store);
            } else if (packet.first == MinecraftPacketIds::ClientboundDataDrivenUIShowScreen) {
                ClientboundDataDrivenUIShowScreenPacket screen;
                screen.readHeader(body);
                screen.read(body, owner.getCodecContext());
                screens.push_back(screen);
            }
        }
        require(screens.size() == 2 && screens[0].mScreenId == "minecraft:custom_form"
                        && screens[1].mScreenId == "minecraft:message_box",
                "the data-driven screens were not shown");
        require(stores.size() == 2, "the screen properties were not sent");
        const DataStoreChange &formData = stores[0].mUpdates[0].mChange;
        require(formData.mDataStoreName == "minecraft"
                        && formData.mProperty == "custom_form_data_" + std::to_string(screens[0].mFormId),
                "the form property is misnamed");
        const auto child = [](const DataStoreChangeValue &value, const char *key) -> const DataStoreChangeValue & {
            for (const auto &entry: value.mMapValue) {
                if (entry.first == key)
                    return entry.second;
            }
            throw std::runtime_error(std::string("missing store key ") + key);
        };
        const DataStoreChangeValue &layout = child(formData.mNewValue, "layout");
        require(child(layout, "length").mType == DataStoreChangeValueType::Long
                        && child(layout, "length").mLongValue == 4,
                "the layout length is not a long");
        require(child(child(layout, "0"), "toggle_visible").mBoolValue, "the toggle is not flagged");
        require(child(child(formData.mNewValue, "closeButton"), "label").mStringValue == "Close"
                        && child(child(formData.mNewValue, "closeButton"), "visible").mBoolValue,
                "the close button is wrong");
        require(child(formData.mNewValue, "title").mType == DataStoreChangeValueType::Map,
                "a raw message title is not structured");

        const auto write = [&](const std::string &property, const char *path, DataStoreScalar data, int32_t pathCount) {
            ServerboundDataStorePacket update;
            update.mUpdate.mDataStoreName = "minecraft";
            update.mUpdate.mProperty = property;
            update.mUpdate.mPath = path;
            update.mUpdate.mData = data;
            update.mUpdate.mUpdateCount = 1;
            update.mUpdate.mPathUpdateCount = pathCount;
            BinaryStream stream;
            update.writeWithHeader(stream, owner.getCodecContext());
            static_cast<NetworkHandler::Listener &>(owner).onDataReceived(id, stream.getBuffer());
        };
        DataStoreScalar on;
        on.mType = DataStoreScalarType::Bool;
        on.mBoolValue = true;
        DataStoreScalar click;
        click.mType = DataStoreScalarType::Number;
        click.mNumberValue = 1;
        write(formData.mProperty, "layout[0].toggled", on, 1);
        // a stale path count is answered, not applied
        write(formData.mProperty, "layout[0].toggled", on, 1);
        write(formData.mProperty, "layout[1].onClick", click, 1);
        write(formData.mProperty, "title", on, 1);
        write("message_box_data_" + std::to_string(screens[1].mFormId), "button2.onClick", click, 1);
        const auto close = [&](int32_t formId, ServerboundDataDrivenScreenClosedPacket::CloseReason reason) {
            ServerboundDataDrivenScreenClosedPacket closed;
            closed.mFormId = formId;
            closed.mCloseReason = reason;
            BinaryStream stream;
            closed.writeWithHeader(stream, owner.getCodecContext());
            static_cast<NetworkHandler::Listener &>(owner).onDataReceived(id, stream.getBuffer());
        };
        close(screens[0].mFormId, ServerboundDataDrivenScreenClosedPacket::CloseReason::ClientCanceled);
        close(screens[1].mFormId, ServerboundDataDrivenScreenClosedPacket::CloseReason::ClientCanceled);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        engine.tick(3);
        const std::unique_ptr<json::Value> ddui = result(owner, "ui");
        expect(*ddui, "readOnly", "InvalidObservableError:Expect 'toggled value' observable to be client writable.");
        expect(*ddui, "notShowing",
               "FormVisibilityError:'minecraft:custom_form' cannot be closed because it is not showing.");
        expect(*ddui, "showing", "true");
        expect(*ddui, "modify",
               "InvalidFormModificationError:'minecraft:custom_form' cannot be modified because it is already showing.");
        expect(*ddui, "again", "FormVisibilityError:'minecraft:custom_form' is already showing.");
        expect(*ddui, "clicked", "true");
        expect(*ddui, "closed", "ClientClosed|true|false|true|true|false");
        expect(*ddui, "box", "ClientClosed|1");
        expect(*ddui, "observable", "3|TypeError:Native type conversion failed.");
        expect(*ddui, "primitiveOptions",
               "TypeError:Native optional type conversion failed.|TypeError:Native optional type conversion failed.|"
               "TypeError:Native optional type conversion failed.|none");

        // getFilteredText: no text processor, so the text comes back as sent unless the player turned filtering off
        player.readSyncedClientOptions("{\"TextFilteringDebugTimeoutMilliSeconds\":600000}");
        require(player.isProfanityFilterEnabled() && player.getTextFilteringDebugTimeout() == 600000,
                "the login client options were not read");
        scriptEvent.execute(console, {"ui:filter", "slow"});
        player.readSyncedClientOptions("");
        scriptEvent.execute(console, {"ui:filter", "on"});
        UpdateClientOptionsPacket options;
        options.mHasFilterProfanityChange = true;
        options.mFilterProfanityChange = false;
        BinaryStream optionsStream;
        options.writeWithHeader(optionsStream, owner.getCodecContext());
        static_cast<NetworkHandler::Listener &>(owner).onDataReceived(id, optionsStream.getBuffer());
        require(!player.isProfanityFilterEnabled(), "UpdateClientOptionsPacket did not turn the filter off");
        scriptEvent.execute(console, {"ui:filter", "off"});
        player.setProfanityFilterEnabled(true);

        sent.clear();
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });
        scriptEvent.execute(console, {"ui:legacy"});
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        std::vector<ModalFormRequestPacket> legacyForms;
        for (const auto &packet: sent) {
            if (packet.first != MinecraftPacketIds::ModalFormRequest)
                continue;
            ReadOnlyBinaryStream body(packet.second);
            ModalFormRequestPacket request;
            request.readHeader(body);
            request.read(body, owner.getCodecContext());
            legacyForms.push_back(request);
        }
        require(legacyForms.size() == 1, "the 1.0.0 form was not sent");
        const std::unique_ptr<json::Value> legacyForm = json::parse(legacyForms[0].mFormData);
        const json::Value *legacyDefault = legacyForm->get("content")->mArray[0]->get("default");
        require(legacyDefault != nullptr && legacyDefault->get("rawtext") != nullptr,
                "a 1.x raw message default was read as options");
        // answered here, settled by the ticks below: an extra tick would move the script clock
        answer(legacyForms[0], "[\"x\"]", false);

        // graphics: a single value goes out as one keyframe at time 0, a player override to that player only
        std::vector<GraphicsOverrideParameterPacket> overrides;
        owner.getNetworkHandler().setOutboundFilter([&overrides, &owner](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            if (Packet::peekId(header, sender, client) == MinecraftPacketIds::GraphicsOverrideParameter) {
                ReadOnlyBinaryStream body(data);
                GraphicsOverrideParameterPacket packet;
                packet.readHeader(body);
                packet.read(body, owner.getCodecContext());
                overrides.push_back(packet);
            }
            return false;
        });
        scriptEvent.execute(console, {"gfx:apply"});
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(overrides.size() == 5, "the graphics overrides were not all sent");
        const GraphicsOverrideParameterPacket &zenith = overrides[0];
        require(zenith.mParameterType == GraphicsOverrideParameterType::SkyZenithColor
                        && zenith.mBiomeIdentifier == "minecraft:plains" && !zenith.mHasPlayerIdentifier
                        && zenith.mValues.size() == 1 && zenith.mValues[0].mKey == 0.0f
                        && zenith.mValues[0].mValue.x == 1.0f && zenith.mValues[0].mValue.z == 0.5f && !zenith.mReset,
                "a sky color did not go out as one keyframe");
        const GraphicsOverrideParameterPacket &blend = overrides[1];
        require(blend.mParameterType == GraphicsOverrideParameterType::HorizonBlendMax && blend.mValues.size() == 2
                        && blend.mValues[1].mKey == 0.5f && blend.mValues[1].mValue.y == 0.8f,
                "the blend keyframes were not sent");
        require(overrides[2].mParameterType == GraphicsOverrideParameterType::WavesDepth
                        && overrides[2].mHasFloatValue && overrides[2].mFloatValue == 3.0f
                        && overrides[2].mHasPlayerIdentifier && overrides[2].mPlayerIdentifier == player.getName(),
                "the player water override lost its value or its player");
        require(overrides[3].mParameterType == GraphicsOverrideParameterType::HighlightsGain
                        && overrides[3].mHasVec3Value && overrides[3].mVec3Value.y == 2.0f,
                "the color grading vector was not sent");
        require(overrides[4].mParameterType == GraphicsOverrideParameterType::SunColor && overrides[4].mReset
                        && overrides[4].mValues.empty(),
                "the reset was not sent");
        const std::unique_ptr<json::Value> graphics = result(owner, "graphics");
        const std::string unsupported =
                "InvalidArgumentError:Invalid value passed. Value did not match one of the supported values.";
        expect(*graphics, "range", unsupported);
        expect(*graphics, "color", unsupported);
        expect(*graphics, "key", unsupported);
        expect(*graphics, "type", "TypeError:Native type conversion failed.");
        expect(*graphics, "biome", "TypeError:Native type conversion failed.");
        expect(*graphics, "constructor", "ReferenceError:No constructor for native class 'BiomeWater'.");

        const std::unique_ptr<json::Value> loot = result(owner, "loot");
        expect(*loot, "manager", "true");
        expect(*loot, "zombie", "entities/zombie|3|true|1-1");
        expect(*loot, "entry",
               "true|minecraft:rotten_flesh|1|SetItemCountFunction:0-2,LootingEnchantFunction:0-1");
        expect(*loot, "conditions", "KilledByPlayerOrPetsCondition,RandomChanceWithLootingCondition|0.01");
        expect(*loot, "missing", "undefined");
        expect(*loot, "roll", "true");
        expect(*loot, "dirt", "minecraft:dirtx1");
        expect(*loot, "stoneHand", "undefined");
        expect(*loot, "stonePick", "minecraft:cobblestone");
        expect(*loot, "entityType", "true");
        expect(*loot, "tool", "TypeError:Native type conversion failed.");
        expect(*loot, "constructor", "ReferenceError:No constructor for native class 'LootTable'.");
        expect(*loot, "pack", "custom/gem|minecraft:diamondx3");

        const std::unique_ptr<json::Value> sounds = result(owner, "sounds");
        expect(*sounds, "registry", "true:true");
        expect(*sounds, "definitions", "test:jingle,minecraft:random.orb,test:ticks,test:clock,test:ranged:true");
        expect(*sounds, "jingle", "0.10000000149011612:Someone:Pop|Folk:Tune:undefined:calm");
        expect(*sounds, "plain", "undefined:undefined:undefined");
        expect(*sounds, "quantized", "0.10000000149011612:1.2339999675750732");
        expect(*sounds, "filters", "1:1:2:1:0:InvalidArgumentError");
        expect(*sounds, "instance", "true:test:jingle:true:true:0.10000000149011612:true:0");
        expect(*sounds, "dimension", "true:undefined:undefined:true");
        expect(*sounds, "bounds",
               "ArgumentOutOfBoundsError:ArgumentOutOfBoundsError:ArgumentOutOfBoundsError:ArgumentOutOfBoundsError:"
               "PropertyOutOfBoundsError:PropertyOutOfBoundsError");
        expect(*sounds, "controls", "ok");
        expect(*sounds, "seek", "0.05");
        expect(*sounds, "endless", "false:0.10000000149011612:true");
        expect(*sounds, "completed", "true:false:0.10000000149011612");

        // isBroadcast reaches only players whose streamed view covers the sound, and broadcast_range if declared
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        player.getChunkStreamState().mLastLoaderChunk = ChunkStreamHandler::packChunk(0, 0);
        std::vector<std::pair<MinecraftPacketIds, std::string>> played;
        owner.getNetworkHandler().setOutboundFilter([&played](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            played.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });
        scriptEvent.execute(console, {"sounds:broadcast"});
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        std::string heard;
        for (const auto &packet: played) {
            if (packet.first != MinecraftPacketIds::PlaySound)
                continue;
            ReadOnlyBinaryStream body(packet.second);
            PlaySoundPacket decoded;
            decoded.readHeader(body);
            decoded.read(body, owner.getCodecContext());
            heard += decoded.mSound + "@" + std::to_string((int) decoded.mPosition.x) + ":"
                     + (decoded.mBypassListenerRangeCheck ? "b" : "-") + ";";
        }
        require(heard == "test:ranged@2:b;random.orb@20:b;random.pop@5000:-;",
                "broadcast sounds reached the wrong players: " + heard);
        expect(*loot, "override", "minecraft:emerald");

        // custom commands: in the command map under both names, parsed from a player and gated by cheats
        CommandMap &commands = owner.getCommands();
        Command *echo = commands.getCommand("cmd:echo");
        require(echo != nullptr && commands.getCommand("echo") == echo, "cmd:echo is not reachable by its alias");
        require(commands.getCommand("cmd:give") != nullptr
                        && commands.getCommand("give") != commands.getCommand("cmd:give"),
                "a custom command took a vanilla name");
        require(commands.getCommand("cmd:bad") == nullptr && commands.getCommand("cmd:late") == nullptr,
                "a refused custom command was registered");
        require(commands.dispatch(playerOrigin, "echo 2 blue 1 2 3 false @s minecraft:stone"),
                "a player could not run cmd:echo");
        require(!commands.dispatch(playerOrigin, "cmd:echo 1.5 red"), "an integer parameter took a float");
        require(!commands.dispatch(playerOrigin, "cmd:echo 1 green"), "an enum parameter took an unknown value");
        require(!commands.dispatch(playerOrigin, "cmd:echo 1"), "a mandatory parameter was skipped");
        std::vector<std::string> outputs;
        owner.getNetworkHandler().setOutboundFilter([&outputs](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            if (Packet::peekId(header, sender, client) == MinecraftPacketIds::CommandOutput)
                outputs.push_back(data);
            return false;
        });
        require(!commands.dispatch(playerOrigin, "cmd:echo -3 red"), "a Failure status counted as success");
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(outputs.size() == 1, "a Failure message was not sent as command output");
        ReadOnlyBinaryStream outputBody(outputs[0]);
        CommandOutputPacket failure;
        failure.readHeader(outputBody);
        failure.read(outputBody, owner.getCodecContext());
        require(failure.mSuccessCount == 0 && failure.mMessages.size() == 1
                        && failure.mMessages[0].mMessageId == "negative" && !failure.mMessages[0].mInternal,
                "a Failure message was not sent as unsuccessful output");
        const bool wasOp = player.getCommandPermission() != CommandPermission::Any;
        player.setOp(false);
        require(!commands.dispatch(playerOrigin, "cmd:give"), "a non-operator ran a GameDirectors custom command");
        // setProperties reopens the worlds: flip the one key in place
        PropertiesSettings &serverProperties = const_cast<PropertiesSettings &>(owner.getProperties());
        serverProperties.setProperty("allow-cheats", "false");
        player.setOp(true);
        require(commands.dispatch(console, "cmd:give"), "the console could not run a cheat command");
        ScriptCommandOrigin scriptNoCheats(world.getOverworld(), nullptr);
        require(commands.dispatch(scriptNoCheats, "cmd:give"), "a script origin could not run a cheat command");
        require(!fromBlockCheats(world.getOverworld()), "a command block ran a cheat command without cheats");
        require(dynamic_cast<ScriptCustomCommand *>(commands.getCommand("cmd:give"))->requiresCheats(),
                "cheatsRequired does not default to true");
        require(!commands.dispatch(playerOrigin, "cmd:give"), "a cheat command ran without cheats");
        require(commands.dispatch(playerOrigin, "echo 7 red"), "cheatsRequired false was refused without cheats");
        serverProperties.setProperty("allow-cheats", "true");
        player.setOp(false);
        BlockOrigin fromBlock(world.getOverworld(), Vector3i(0, 101, 8));
        require(commands.dispatch(fromBlock, "echo 8 red"), "a command block could not run cmd:echo");

        // the commands reach the client with their aliases, parameter types and enums
        std::vector<std::string> advertised;
        owner.getNetworkHandler().setOutboundFilter([&advertised](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            if (Packet::peekId(header, sender, client) == MinecraftPacketIds::AvailableCommands)
                advertised.push_back(data);
            return false;
        });
        LoginHandler::sendAvailableCommands(owner, player);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(advertised.size() == 1, "no AvailableCommandsPacket");
        ReadOnlyBinaryStream commandsBody(advertised[0]);
        AvailableCommandsPacket available;
        available.readHeader(commandsBody);
        available.read(commandsBody, owner.getCodecContext());
        const CommandData *echoData = nullptr;
        bool adminSent = false;
        for (const CommandData &data: available.mCommands) {
            if (data.mName == "cmd:echo")
                echoData = &data;
            adminSent = adminSent || data.mName == "cmd:give";
        }
        require(echoData != nullptr && !adminSent, "custom commands were not filtered by permission");
        require(echoData->mDescription == "echo" && echoData->mHasAliases
                        && echoData->mAliases.mValues == std::vector<std::string>({"cmd:echo", "echo"}),
                "cmd:echo was sent without its alias");
        require(echoData->mOverloads.size() == 1 && echoData->mOverloads[0].mParameters.size() == 6,
                "cmd:echo was sent without its parameters");
        const std::vector<CommandParamData> &sentParameters = echoData->mOverloads[0].mParameters;
        require(sentParameters[0].mType == CommandParamType::Int && !sentParameters[0].mOptional,
                "the Integer parameter was not sent as an int");
        require(sentParameters[1].mHasEnumData && sentParameters[1].mEnumData.mName == "cmd:color"
                        && sentParameters[1].mEnumData.mValues == std::vector<std::string>({"red", "blue"}),
                "the script enum was not sent");
        require(sentParameters[2].mType == CommandParamType::Position && sentParameters[2].mOptional,
                "the Location parameter was not sent as a position");
        require(sentParameters[3].mHasEnumData && sentParameters[3].mEnumData.mName == "Boolean",
                "the Boolean parameter was not sent as the Boolean enum");
        require(sentParameters[4].mType == CommandParamType::Target, "the selector was not sent as a target");
        require(sentParameters[5].mHasEnumData && sentParameters[5].mEnumData.mName == "Block",
                "the BlockType parameter was not sent as the Block enum");
        // the script origin: permission level 1, its own name, one count per output
        ScriptCommandOrigin fromScript(world.getOverworld(), nullptr);
        require(fromScript.getCommandPermission() == CommandPermission::GameDirectors,
                "the script origin does not run at level 1");
        require(fromScript.getSenderName() == "Script Engine", "the script origin is not named Script Engine");
        fromScript.sendTranslation("commands.kill.successful", {"a"});
        fromScript.sendTranslation("commands.kill.successful", {"b"});
        fromScript.sendTranslation("commands.generic.noTargetMatch", {});
        require(fromScript.getOutputCount() == 2, "an error output counted as a success");
        Vector3f local;
        require(Command::parsePosition({"^", "^", "^2"}, 0, Vector3f(1.0f, 2.0f, 3.0f), Vector3f(0.0f, 0.0f, 0.0f),
                                       local)
                        && std::fabs(local.x - 1.0f) < 0.001f && std::fabs(local.z - 5.0f) < 0.001f,
                "^ ^ ^2 did not move forward along +z");
        require(Command::parsePosition({"^1", "^", "^"}, 0, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 90.0f, 0.0f),
                                       local)
                        && std::fabs(local.z - 1.0f) < 0.001f && std::fabs(local.x) < 0.001f,
                "^1 is not to the left facing west");
        require(Command::parsePosition({"^", "^1", "^"}, 0, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 0.0f, 0.0f),
                                       local)
                        && std::fabs(local.y - 1.0f) < 0.001f,
                "^ ^1 ^ did not move up");
        require(!Command::parsePosition({"^", "~", "^"}, 0, Vector3f(), Vector3f(), local), "^ mixed with ~ parsed");
        ScriptCommandOrigin fromCow(world.getOverworld(), cow);
        const std::vector<ServerActor *> selfTargets = owner.resolveActorTargets(fromCow, "@s");
        require(selfTargets.size() == 1 && selfTargets[0] == cow, "@s of a cow did not resolve to it");
        require(owner.resolveActorTargets(fromCow, "@s[type=minecraft:pig]").empty(), "@s ignored its type filter");
        ExecuteCommandOrigin executedFromCow(fromCow, nullptr, cow->getPosition(), cow->getRotation(),
                                             &world.getOverworld());
        const std::vector<ServerActor *> executedTargets = owner.resolveActorTargets(executedFromCow, "@s");
        require(executedTargets.size() == 1 && executedTargets[0] == cow, "execute lost the cow running it");
        require(commands.dispatch(playerOrigin, "beta:toggle off"), "an enumName parameter did not parse");
        player.setOp(wasOp);

        // an entity container opens with the actor's id, saves as ChestItems and loads back
        std::vector<ContainerOpenPacket> opened;
        owner.getNetworkHandler().setOutboundFilter([&opened, &owner](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            if (Packet::peekId(header, sender, client) == MinecraftPacketIds::ContainerOpen) {
                ReadOnlyBinaryStream body(data);
                ContainerOpenPacket packet;
                packet.readHeader(body);
                packet.read(body, owner.getCodecContext());
                opened.push_back(packet);
            }
            return false;
        });
        InventoryManager &windows = player.getInventoryManager();
        require(windows.openEntityContainer(*minecart, ContainerType::MinecartChest),
                "the chest minecart did not open");
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(opened.size() == 1 && opened.front().mUniqueActorId == minecart->getUniqueId()
                        && opened.front().mType == ContainerType::MinecartChest,
                "ContainerOpenPacket does not name the minecart");
        require(windows.getContainer() == minecart->getEntityContainer()
                        && windows.getContainer()->getContainerItem(4).mCount == 7,
                "the open window does not reach the minecart inventory");
        require(!windows.openEntityContainer(*cow, ContainerType::Horse), "a cow opened an inventory");
        windows.onClientRemoveWindow(windows.getContainerWindowId());
        require(windows.getContainerActorId() == -1 && !windows.isContainerOpen(), "the window did not close");
        const Vector3i stoneAt(0, 101, 8);
        engine.onBlockContainerOpened(owner.getLevelFor(player), stoneAt, player);
        engine.onBlockContainerClosed(owner.getLevelFor(player), stoneAt, player);
        engine.onPlayerStartBreakingBlock(player, stoneAt, 1);
        engine.onPlayerCancelBreakingBlock(player, stoneAt, 1, 1.5);
        engine.onItemStartUseOn(player, stoneAt, 1);
        engine.onItemStopUseOn(player, stoneAt);
        const Tag savedCart = minecart->saveNbt();
        const Tag *chestItems = savedCart.get("ChestItems");
        require(chestItems != nullptr && chestItems->getList().size() == 1
                        && chestItems->getList().front().getByte("Slot") == 4,
                "the minecart items were not saved as ChestItems");
        ChestVehicleActor reloaded(9999, "minecraft:chest_minecart", ActorSize{0.98f, 0.7f}, true);
        Tag legacy = Tag::ofCompound();
        legacy.put("Items", *chestItems);
        reloaded.loadEquipment(legacy, owner.getCodecContext());
        require(reloaded.getEntityContainer()->getContainerItem(4).mCount == 7, "Items did not load");

        // the script edits land in the game's NBT, as the game saves them
        const ItemStack &edited = player.getInventory().getItem(5);
        require(edited.mDefinition != nullptr && edited.mDefinition->getIdentifier() == "minecraft:diamond_sword"
                        && edited.mDamage == 10,
                "the scripted sword did not reach the inventory");
        const Tag *display = edited.mTag.get("display");
        require(display != nullptr && display->getString("Name") == "Renamed"
                        && display->get("Lore")->getList().size() == 2,
                "the name tag and lore were not written to display");
        const Tag *enchantments = edited.mTag.get("ench");
        require(enchantments != nullptr && enchantments->getList().size() == 1
                        && enchantments->getList().front().getShort("id") == 9
                        && enchantments->getList().front().getShort("lvl") == 3,
                "the enchantment was not written to ench");
        require(edited.mTag.getByte("minecraft:keep_on_death") == 1 && edited.mTag.getByte("minecraft:item_lock") == 1
                        && edited.mCanBreak.size() == 1 && edited.mCanBreak.front() == "minecraft:stone"
                        && edited.mTag.get("DynamicProperties") != nullptr,
                "keepOnDeath, lockMode, canDestroy or dynamic properties were lost");
        require(player.getInventory().getArmor(PlayerInventory::ARMOR_HEAD).mDefinition->getIdentifier()
                                == "minecraft:diamond_helmet"
                        && player.getInventory().getOffhand().mDefinition->getIdentifier() == "minecraft:shield",
                "setEquipment did not reach the armor and offhand");
        require(EnderChestInventoryStore::getInstance().get(player.getUniqueId()).getContainerItem(0).mCount == 4,
                "the ender inventory was not written");
        require(piglinMob->getEquipment().getSlot(MobEquipment::MAINHAND).mDefinition->getIdentifier()
                                == "minecraft:golden_sword"
                        && piglinMob->getEquipment().getInventoryItem(1).mCount == 3,
                "the piglin equipment or inventory was not written");

        require(wolfMob->getEntityContainer() == nullptr, "a wolf opens an inventory");
        if (static_cast<MobActor *>(horse)->isTamed()) {
            Container *saddleBags = horse->getEntityContainer();
            require(saddleBags != nullptr && saddleBags->getContainerSize() == 2,
                    "a tamed horse has no two slot inventory to open");
            {
                ServerActor *copy = owner.spawnActor(world.getOverworld(), "minecraft:horse",
                                                     Vector3f(-8.5f, 100.0f, 4.5f));
                MobActor *loaded = dynamic_cast<MobActor *>(copy);
                require(loaded != nullptr && !loaded->isTamed(), "no second horse");
                loaded->loadNbt(horse->saveNbt());
                require(loaded->isTamed() && loaded->getOwnerId() == MobActor::NO_OWNER,
                        "an ownerless tame was not saved");
                owner.removeActor(copy->getUniqueId());
            }
        }
        // the player is erased below: later checks keep its id, never the reference
        const int64_t playerUniqueId = player.getUniqueId();
        engine.beforePlayerLeave(player);
        owner.getPlayers().erase(id);
        for (int64_t tick = 4; tick <= 8; ++tick)
            engine.tick(tick);

        const std::unique_ptr<json::Value> filtered = result(owner, "ui");
        expect(*filtered, "filterAsync_on", "true");
        expect(*filtered, "filter_on", "\"hello\"");
        expect(*filtered, "filter_off", "[\"DisabledByPlayer\"]");
        require(filtered->get("filter_slow") == nullptr, "the client debug timeout did not hold the answer back");
        const std::unique_ptr<json::Value> legacyUi = result(owner, "legacyUi");
        expect(*legacyUi, "members", "undefined|undefined|undefined|false|function");
        expect(*legacyUi, "positional", "object");
        expect(*legacyUi, "response", "false|false|[\"x\"]");

        const BlockState stateful = level.getBlockState(at.x, at.y, at.z);
        require(stateful.mName == "test:stateful" && stateful.mStates.get("test:lit") != nullptr
                        && stateful.mStates.get("test:lit")->asByte() == 1
                        && stateful.mStates.get("test:level")->asInt() == 1,
                "setPermutation did not keep the states");
        engine.onCustomBlockTick(level, "test:stateful", at);

        WorldClock *clock = engine.worldClocks().find("test:clock");
        require(clock != nullptr, "the script clock was not registered");
        engine.worldClocks().setPaused(*clock, false);
        engine.worldClocks().setTime(*clock, INT32_MAX);
        engine.tick(9);
        require(clock->mTime == 5, "worldClockOnRestart did not set the new time");

        engine.worldClocks().setPaused(*clock, true);
        const Tag saved = engine.worldClocks().save();
        const Tag *found = nullptr;
        for (const Tag &entry: saved.get("clocks")->getList()) {
            if (entry.get("Name")->asString() == "test:clock")
                found = &entry;
        }
        require(found != nullptr, "the script clock was not saved");
        const Tag &entries = *found;
        require(entries.get("Name")->asString() == "test:clock" && entries.get("Time")->asInt() == 5
                        && entries.get("IsPaused")->asByte() == 1,
                "the clocks were not saved in the game's form");
        WorldClockSystem restored;
        restored.load(saved);
        bool markerError = false;
        require(restored.registerClock("test:clock", {}, markerError) == WorldClockError::None
                        && restored.find("test:clock")->mTime == 5 && restored.find("test:clock")->mPaused,
                "a registered clock did not take its saved state");
        int64_t levelTime = 1000;
        bool levelPaused = false;
        WorldClockSystem::LevelTime binding;
        binding.mGet = [&levelTime]() { return levelTime; };
        binding.mSet = [&levelTime](int64_t value) { levelTime = value; };
        binding.mSetPaused = [&levelPaused](bool paused) { levelPaused = paused; };
        restored.bindOverworld(binding);
        WorldClock *overworldClock = restored.find("minecraft:overworld");
        require(overworldClock->mTime == 1000, "the overworld clock does not start at the level's time");
        levelTime = 1001;
        restored.tick();
        require(overworldClock->mTime == 1001, "the overworld clock does not follow the level's time");
        restored.setTime(*overworldClock, 6000);
        require(levelTime == 6000, "setting the overworld clock does not set the level's time");
        restored.setPaused(*overworldClock, true);
        require(levelPaused, "pausing the overworld clock does not stop the level's day");
        restored.setPaused(*overworldClock, false);
        SyncWorldClocksPacket states;
        restored.writeStates(states);
        require(states.mType == SyncWorldClocksType::SyncState && states.mClockStates.size() == 2
                        && states.mClockStates.front().mClockId == 0x63D7EDE73C38F916ull,
                "the periodic sync does not carry every clock under its hashed id");

        const std::unique_ptr<json::Value> entities = result(owner, "entities");
        const json::Value &e = *entities;
        if (e.get("error") != nullptr)
            require(false, "the entities pack threw: " + text(e, "error"));
        expect(e, "idBefore", std::to_string(loadedId - 77));
        expect(e, "idAfter", "true:" + std::to_string(loadedId));
        expect(e, "enums", "void,entityAttack,Born,SelfHeal,Jump,Pressed,Touch,Attack,2,2");
        if (e.get("sbError") != nullptr)
            require(false, "the scoreboard part threw: " + text(e, "sbError"));
        expect(e, "sbClasses", "true,true,ReferenceError: No constructor for native class 'ScoreboardObjective'.");
        expect(e, "sbNone", "true");
        expect(e, "sbObjective", "test:kills,Kills,true,test:kills,undefined");
        expect(e, "sbDuplicate", "Error: Failed to add objective 'test:kills' as it is already being tracked.");
        expect(e, "sbAdd", "7,3,-1");
        expect(e, "sbIdentity", "true,Player,ScriptPlayer,true,true,number");
        expect(e, "sbCalf", "Entity,\"\"");
        expect(e, "sbScores", "true:Player=7;true:FakePlayer=3;true:Entity=-1");
        expect(e, "sbGet", "7,undefined,true,true");
        expect(e, "sbOverflow",
               "Error: Encountered an integer overflow when trying to modify objective score for "
               "participant 'fake'.");
        expect(e, "sbQuery", "1,1,1,1,0");
        expect(e, "sbDisplay", "true,test:kills,0");
        expect(e, "sbParticipants", "3,3");
        expect(e, "sbRemoveParticipant", "true,false");
        expect(e, "sbBadSlot", "TypeError: Native type conversion failed.");
        expect(e, "sbClear", "test:kills,true");
        expect(e, "sbRemove", "true,false,Error: Failed to find objective 'test:other'.,false");
        expect(e, "sbStale", "Error: Failed to set invalid objective at DisplaySlot.");
        expect(e, "sbReAdded", "false");
        expect(e, "sbEnums", "BelowName,1,FakePlayer");
        {
            Scoreboard &board = owner.getScoreboard();
            const ScoreboardObjective *kills = board.getObjective("test:kills");
            const ScoreboardIdentity *kept = board.findFakePlayer("kept");
            require(kills != nullptr && kept != nullptr && *kills->find(kept->mId) == 42
                            && board.getDisplay("belowname") != nullptr
                            && board.getDisplay("belowname")->mObjective == "test:kills",
                    "the script scoreboard did not reach the server scoreboard");
            const ScoreboardIdentity *scripted = board.findActor(playerUniqueId);
            require(scripted != nullptr && scripted->mType == ScoreboardIdentityType::Player
                            && *kills->find(scripted->mId) == 7,
                    "the player's script score is not the server's");
        }
        expect(e, "componentIds", "minecraft:health,minecraft:type_family,minecraft:movement");
        expect(e, "componentChain", "true");
        expect(e, "typesConstructor", "ReferenceError");
        expect(e, "cowFound", "true");
        expect(e, "families", "cow,mob");
        expect(e, "familyQuery", "2,0");
        expect(e, "health", "true:10:10:true");
        expect(e, "healthSet", "4:false:false:4");
        expect(e, "componentsCount", "true");
        expect(e, "movement", "true:true");
        expect(e, "missing", "true");
        expect(e, "navigation", "true:undefined");
        expect(e, "localization", "entity.cow.name,entity.cow.name,minecraft:cow,true,true");
        expect(e, "effectTypes", "speed,true,true");
        expect(e, "effectName", "Speed II|potion.digSpeed");
        expect(e, "addEffect", "true:1:40:true");
        expect(e, "cancelledEffect", "true");
        expect(e, "effects", "speed");
        expect(e, "removedEffect", "false");
        expect(e, "propertyChecks",
               "InvalidArgumentError: Invalid value passed to argument [0]. Property \"test:none\" does not exist on "
               "Entity of type \"minecraft:cow\".|InvalidArgumentError: Invalid type passed to argument [0]. Expected "
               "type: string property|InvalidArgumentError: Invalid type passed to argument [1]. Expected type: enum "
               "property|ok|warm");
        expect(e, "aabb", "true:true");
        expect(e, "sneaking", "true");
        expect(e, "hurt", "true");
        expect(e, "projectileOption", "TypeError");
        expect(e, "rotation", "10,20");
        expect(e, "lookAt", "0");
        expect(e, "entityRay", "2:false:5.74:0:1");
        expect(e, "blockRay", "true");
        expect(e, "shapeRay", "minecraft:oak_fence:North:0.375");
        expect(e, "filterRay", "8:8:true:true");
        expect(e, "tameMount", "true:false:true");
        expect(e, "emoting", "false");
        expect(e, "betaAbsent", "true");
        expect(e, "op", "1:2");
        expect(e, "pending", "rejected");
        expect(e, "blockedTeleport", "false");
        expect(e, "teleport", "true:200:90");
        expect(e, "teleportBounds", "ok");
        expect(e, "entityDynamic", "two:true");
        expect(e, "resetProperty", "function");
        expect(e, "matchesLevel", "true,false,false,true");
        expect(e, "player", "false,false,false,0,0,7,1,2");
        expect(e, "input", "KeyboardAndMouse:Released:{\"x\":0,\"y\":0}");
        expect(e, "slot", "4:InvalidArgumentError");
        expect(e, "spawnBefore", "true");
        expect(e, "spawn", "5,70,6,true");
        expect(e, "spawnCleared", "true");
        expect(e, "playerFamilies", "player:true");
        expect(e, "playerHealth", "20:true");
        expect(e, "seat", "true:181:0:1.10:0");
        expect(e, "feed", "8:true:wheat:2::0");
        expect(e, "ageable", "wheat@true:minecraft:ageable_grow_up:true");
        expect(e, "addItem",
               "InvalidEntityComponentError: Attempting to access invalid entity component minecraft:inventory.|true");
        expect(e, "overrides", "Error: Property \"test:none\" does not exist on Entity of type \"minecraft:cow\".|ok");
        expect(e, "device", "2:96:Mobile::Deferred:true:number:true");
        expect(e, "spline",
               "true|2|Error: Linear needs at least 2 control points|"
               "Error: CatmullRom needs at least 3 control points|"
               "Error: First progress key frame must be at time 0.0|"
               "Error: Time between rotation frames must be greater than 0.05|"
               "Error: Linear needs at least 2 control points");
        expect(e, "fog",
               "0|1|2|FogSettingsError: Failed to modify fog settings due to an invalid fog identifier.|"
               "minecraft:fog_bamboo_jungle,Test:Mist,test:haze|untagged,a|test:haze|Test:Mist|false|true||"
               "FogSettingsError: Failed to modify fog settings because the limit of 16 fog settings has "
               "been reached.");
        expect(e, "fogStack", "a:b,c:d/t");
        // the game's unordered_set order, rehashed once past eight tags
        expect(e, "fogTags", "t1,untagged,t0,t2,t3,t4,t5,t6,t7,t8,t9");
        expect(e, "controlScheme",
               "LockedPlayerRelativeStrafe|EngineError: An internal engine error occurred.|"
               "InvalidArgumentError: Invalid value passed. Control Scheme 'CameraRelative' is not "
               "allowed for current camera: [minecraft:first_person]|"
               "TypeError: Native type conversion failed.|ok|PlayerRelative|ok|"
               "LockedPlayerRelativeStrafe");
        expect(e, "splinePlay",
               "ok|TypeError: Native type conversion failed.|TypeError: Native type conversion failed.");
        expect(e, "gameMode", "Creative");
        expect(e, "tickingClass", "true");
        expect(e, "ticking", "true:1:2:2");
        expect(e, "tickingDuplicate", "TickingAreaError:IdentifierAlreadyExists");
        expect(e, "tickingLimits",
               "1000:TickingAreaError:SideLengthExceeded:TickingAreaError:OverChunkLimit:false:true:"
               "TypeError:undefined:false");
        expect(e, "tickingRemoved", "false");
        expect(e, "killed", "true");
        expect(e, "events",
               "spawn:minecraft:cow:Spawned;spawn:minecraft:cow:Born;spawn:minecraft:wolf:Spawned;"
               "beforeTame:minecraft:wolf;tamed:minecraft:wolf:true;spawn:minecraft:horse:Spawned;spawn:minecraft:piglin:Spawned;spawn:minecraft:chest_minecart:Spawned;health:20>10;"
               "health:10>12;heal:2:Regeneration;health:12>11;hurt:minecraft:player:1:magic:false;emote:emote:wave;button:Jump:Pressed;"
               "mode:KeyboardAndMouse>Touch;swing:Attack;sneak:minecraft:player;unsneak:minecraft:player;"
               "interact:minecraft:cow;dimension:minecraft:overworld>minecraft:nether;effect:speed:40:true;"
               "dimension:minecraft:overworld>test:void;dimension:test:void>minecraft:overworld;"
               "effect:speed:40:true;effect:haste:200:true;hurt:minecraft:cow:3:fall:true;"
               "trigger:minecraft:ageable_grow_up:[{\"addedComponentGroups\":[\"minecraft:cow_adult\"],"
               "\"removedComponentGroups\":[\"minecraft:cow_baby\"]}];"
               "gm:Survival>Creative;die:minecraft:cow:override;spawn:minecraft:horse:Spawned;"
               "beforeRemove:minecraft:horse;remove:minecraft:horse;leaving:true;");

        const std::unique_ptr<json::Value> items = result(owner, "items");
        const json::Value &it = *items;
        if (it.get("error") != nullptr)
            require(false, "the items pack threw: " + text(it, "error"));
        expect(it, "constructed", "minecraft:diamond_sword:1:1:false:true:minecraft:diamond_sword");
        expect(it, "badType", "TypeError");
        expect(it, "badAmount", "ArgumentOutOfBoundsError");
        expect(it, "clamped", "16");
        expect(it, "meta", "Blade:2:second:first:true:slot");
        expect(it, "loreBounds", "ArgumentOutOfBoundsError");
        expect(it, "canDestroy", "minecraft:stone:TypeError");
        expect(it, "dynamic", "me:TypeError");
        expect(it, "clone", "2:true:Blade:me");
        expect(it, "stackable", "true:false:true");
        expect(it, "durability", "true:1562:10:ArgumentOutOfBoundsError:ArgumentOutOfBoundsError");
        expect(it, "enchant", "true:1:3:false:Sword");
        expect(it, "enchantErrors",
               "EnchantmentTypeUnknownIdError,EnchantmentLevelOutOfBoundsError,"
               "EnchantmentTypeNotCompatibleError,false,true");
        expect(it, "enchantTypes", "minecraft:flame:1:true:true:Sword");
        expect(it, "components", "minecraft:durability,minecraft:enchantable");
        expect(it, "food", "true:4:0.3:false:true");
        expect(it, "cooldown", "true:minecraft:ender_pearl:true");
        expect(it, "potion",
               "minecraft:splash_potion:true:minecraft:swiftness:ThrownSplash:3600:true:3:"
               "InvalidPotionEffectTypeError");
        expect(it, "bookBefore", "true:5:inserted:true:raw:false:BookPageContentError");
        expect(it, "bookTitle", "BookError:ExceedsTitleLength");
        expect(it, "bookSigned", "minecraft:written_book:true:Story:ScriptPlayer");
        expect(it, "bundle", "true:3:1:3:2");
        expect(it, "inventory", "true:true:36:36:36");
        expect(it, "container", "true:Blade:0:12:3:true:ArgumentOutOfBoundsError");
        expect(it, "slot", "true:true:Renamed:minecraft:diamond_sword:undefined:true:2");
        expect(it, "moved", "1:true:2");
        expect(it, "transferred", "true:true");
        expect(it, "equipment", "true:minecraft:diamond_helmet:minecraft:shield:3:2");
        expect(it, "ender", "true:27:4");
        expect(it, "cursor", "true:false");
        expect(it, "playerComponents",
               "minecraft:cursor_inventory,minecraft:ender_inventory,minecraft:equippable,"
               "minecraft:inventory");
        expect(it, "mob", "8:none:false:3:minecraft:golden_sword:true");
        expect(it, "itemTypes", "true:item.apple.name");
        expect(it, "dynamicMore", "test:a,test:b:true");
        expect(it, "toolMore", "0:minecraft:dirt:true:{\"min\":100,\"max\":100}:50:true:false");
        expect(it, "enchantMore", "minecraft:unbreaking1");
        expect(it, "enchantCleared", "0:Pickaxe");
        expect(it, "dye", "true:true");
        expect(it, "compost", "30");
        expect(it, "bookMore", "a,~:~,b:true:true");
        expect(it, "deadBook",
               "false;minecraft:book;InvalidItemStackError|true|minecraft:writable_book|Failed to get property "
               "'pageCount' due to the ItemStack being invalid (has the ItemStack been removed?).;"
               "InvalidItemStackError|true|minecraft:writable_book|Failed to call function 'getPageContent' due to "
               "the ItemStack being invalid (has the ItemStack been removed?).");
        expect(it, "deadProperties",
               "false;InvalidItemStackError|true|minecraft:chest|Failed to call function 'totalByteCount' due to "
               "the ItemStack being invalid (has the ItemStack been removed?).");
        expect(it, "liveBook", "true;2");
        expect(it, "potionsMore", "49:Consume:true:minecraft:potion");
        expect(it, "cooldownMore", "20:true");
        expect(it, "inventoryMore", "inventory:false:false:false:0:true");
        expect(it, "cursorMore", "ok:true:true:true");
        expect(it, "rules", "ContainerRulesError:BannedItem");
        expect(it, "cart", "27:minecart_chest:true:27:7:true");
        expect(it, "blockProperties", "5:true:5:true");
        expect(it, "blockPropertiesCleared", "undefined");
        expect(it, "stateMatches", "true:false:false:false");
        expect(it, "blockType", "minecraft:stone:true:true");
        expect(it, "events",
               "complete:test:wand:ScriptPlayer;complete:test:snack:ScriptPlayer;start:minecraft:apple:32;release:minecraft:bow:5;stop:minecraft:bow:5;"
               "complete:minecraft:apple:ScriptPlayer;beforePickup:minecraft:player:minecraft:item;"
               "pickup:minecraft:player:minecraft:diamondx2;beforePickup:minecraft:piglin:minecraft:item;"
               "change:3:Hotbar:->minecraft:diamond;hotbar:3;opened:minecraft:chest_minecart:minecraft:player;"
               "closed:minecraft:chest_minecart:minecraft:player;");
        expect(it, "world",
               "opened:minecraft:stone:minecraft:player:true;closed:minecraft:stone:minecraft:player;"
               "start:minecraft:stone:Up:minecraft:player;cancel:minecraft:stone:1;"
               "useOn:minecraft:stone:Up:minecraft:player;stopUseOn:minecraft:stone;");
        expect(it, "input", "2:false:minecraft:player;2:true:minecraft:player;");

        const std::unique_ptr<json::Value> betaEntities = result(owner, "betaEntities");
        expect(*betaEntities, "cursor", "grab:minecraft:diamond;release:minecraft:diamond:ScriptPlayer;");
        expect(*betaEntities, "target", "true");
        expect(*betaEntities, "betaPlayer", "true");
        expect(*betaEntities, "instrument",
               "true:note.pling:note.none:InvalidArgumentError:TypeError:ok:PropertyOutOfBoundsError:true");
        expect(*betaEntities, "chat", "[A] ScriptPlayer!:§a");
        expect(*betaEntities, "chatCleared", "[A] ScriptPlayer:true");
        expect(*betaEntities, "eat", "true");
        expect(*betaEntities, "eatStone", "Error");
        expect(*betaEntities, "ping", "EngineError");
        expect(*betaEntities, "customInstance", "true:{\"value\":42,\"list\":[\"a\",true]}:true:true:true:{}:true");
        expect(*betaEntities, "packDye", "true:true:255,0,0:23,88,130:true");

        const std::unique_ptr<json::Value> gaps = result(owner, "gaps");
        expect(*gaps, "molangEarly",
               "ReferenceError: Native constructor for [MolangVariableMap::constructor] cannot be used in early execution.");
        expect(*gaps, "triggerEarly",
               "ReferenceError: Native constructor for [Trigger::constructor] cannot be used in early execution.");
        expect(*gaps, "molang", "true");
        expect(*gaps, "molangDot", "Error: Variable name cannot contain '.' unless it is preceded by 'variable'.");
        expect(*gaps, "molangType", "TypeError: Native type conversion failed.");
        expect(*gaps, "constants", "20,24000,8,0,6");
        expect(*gaps, "trigger", "test:grow,true");
        expect(*gaps, "enums", "AnyItem,Console,Deferred,PlayerRelativeStrafe,IgnoreWaterlogging");
        expect(*gaps, "serializable", "true");
        expect(*gaps, "dimensionTypes", "minecraft:overworld,minecraft:nether,minecraft:the_end,test:void");
        expect(*gaps, "nether", "minecraft:nether,undefined");
        expect(*gaps, "itemKey", "item.apple.name,tile.stone.name");
        expect(*gaps, "volume", "0|true|false|6|2");
        expect(*gaps, "music", "PropertyOutOfBoundsError: ");
        expect(*gaps, "musicOk", "ok,ok,ok");
        expect(*gaps, "particle", "ok");
        expect(*gaps, "primitives",
               "1000|true|false|2|1|true|TypeError: Native type conversion failed.|hello|"
               "{\"text\":\"far\"}");
        expect(*gaps, "primitivesLeft", "1");
        expect(*gaps, "primitiveLimit",
               "PrimitiveShapeError: PrimitiveShapesManager cannot add more shapes, limit reached: 1000");
        expect(*gaps, "primitivesCleared", "0");
        expect(*gaps, "particleBadMap", "TypeError: Native type conversion failed.");

        const std::unique_ptr<json::Value> legacyPack = result(owner, "legacy");
        expect(*legacyPack, "dimensions", "minecraft:overworld,minecraft:nether,minecraft:the_end");
        expect(*legacyPack, "componentIds", "minecraft:can_climb,minecraft:inventory");
        expect(*legacyPack, "orphans", "function,function");
        expect(*legacyPack, "immediate", "true");
        expect(*legacyPack, "async", "resolved:1");
        expect(*legacyPack, "asyncError", "CommandError");
        expect(*legacyPack, "worldSound", "ok,ok,threw");

        ScriptMolangMap molang;
        molang.mVariables.push_back({"variable.size", 0.5f, false, {}});
        molang.mVariables.push_back({"variable.tint", 0.0f, true, {{".r", 1.0f}, {".g", 0.25f}}});
        require(molang.toJson()
                        == "[{\"name\":\"variable.size\",\"value\":{\"type\":\"float\",\"value\":0.5}},"
                           "{\"name\":\"variable.tint\",\"value\":{\"type\":\"member_array\",\"value\":["
                           "{\"name\":\".r\",\"value\":{\"type\":\"float\",\"value\":1}},"
                           "{\"name\":\".g\",\"value\":{\"type\":\"float\",\"value\":0.25}}]}}]",
                "the molang map JSON is not the game's: " + molang.toJson());

        const std::unique_ptr<json::Value> stable2 = result(owner, "stable2");
        expect(*stable2, "alone", "true");
        expect(*stable2, "internals", "0");
        expect(*stable2, "early",
               "ReferenceError: Native function [World::getAllPlayers] cannot be used in early execution.");
        expect(*stable2, "earlyRun", "ok");
        expect(*stable2, "entityConstructor", "ReferenceError: No constructor for native class 'Entity'.");
        expect(*stable2, "playerExtendsEntity", "true");
        expect(*stable2, "worldIsWorld", "true");
        expect(*stable2, "gameMode", "Survival,Spectator");
        expect(*stable2, "sneak", "5,12");
        expect(*stable2, "inventoryType", "Inventory,Hotbar");
        expect(*stable2, "componentId", "minecraft:inventory,minecraft:inventory");
        expect(*stable2, "kick", "false");
        expect(*stable2, "hideAllForms", "false");
        expect(*stable2, "ticking", "true");
        expect(*stable2, "worldInitialize", "false");
        expect(*stable2, "itemUseOn", "false");
        expect(*stable2, "chatSend", "false");
        expect(*stable2, "handle", "0");
        expect(*stable2, "legacyExport", "false");
        expect(*stable2, "engineError", "ReferenceError: No constructor for native class 'EngineError'.");
        expect(*stable2, "invalidArgument", "true");
        expect(*stable2, "enums", "Hard,6000,4,Thunder,NPCDialogue,keepInventory,4");
        expect(*stable2, "signals", "65,15,1,2");
        expect(*stable2, "v1Signals", "true");
        expect(*stable2, "gameRule", "randomTickSpeed=3,number");
        expect(*stable2, "weatherBefore", "Clear>Rain;Clear>Rain;");
        expect(*stable2, "weatherAfter", "minecraft:overworld|Clear|Thunder;");
        expect(*stable2, "scriptEvents",
               "test:ping|hello world|Server|false|true|true;other:pong||Entity|true|true|true;ui:forms||Server|false|true|true;"
               "ui:ddui||Server|false|true|true;ui:filter|slow|Server|false|true|true;ui:filter|on|Server|false|true|true;"
               "ui:filter|off|Server|false|true|true;ui:legacy||Server|false|true|true;gfx:apply||Server|false|true|true;"
               "sounds:broadcast||Server|false|true|true;test:event|m|Server|false|true|true;");
        expect(*stable2, "filtered", "other:pong;");
        require(stable2->get("removed") == nullptr, "an unsubscribed scriptEventReceive callback ran");
        expect(*stable2, "prototypeBound",
               "ReferenceError: Native function [Entity::addTag] object bound to prototype does not exist.");
        expect(*stable2, "startupEarly",
               "ReferenceError: Native function [World::getAllPlayers] cannot be used in early execution.");
        expect(*stable2, "register", "ok");
        expect(*stable2, "lateRegister",
               "CustomComponentInvalidRegistryError: Cannot register custom component "
               "outside of the ModuleStartup event.");
        expect(*stable2, "normal", "ok");
        expect(*stable2, "item", "true");
        expect(*stable2, "dimension", "true");
        expect(*stable2, "restrictedWrite",
               "ReferenceError: Native function [Entity::addTag] cannot be used in restricted execution.");
        expect(*stable2, "restrictedRead", "ok");
        expect(*stable2, "restrictedRun", "ok");
        expect(*stable2, "amplifier", "false");
        expect(*stable2, "player", "true");
        expect(*stable2, "same", "true");
        expect(*stable2, "escape", "blocked");
        expect(*stable2, "invalid",
               "InvalidEntityError: Failed to get property 'location' due to Entity being "
               "invalid (has the Entity been removed?).");
        expect(*stable2, "invalidClass", "true");
        expect(*stable2, "isValid", "false");

        const std::unique_ptr<json::Value> worldResult = result(owner, "world");
        const json::Value &w = *worldResult;
        expect(w, "rulesConstructor", "ReferenceError: No constructor for native class 'GameRules'.");
        expect(w, "stableRules", "true");
        expect(w, "earlyRules",
               "ReferenceError: Native property getter [World::gameRules] cannot be used in early execution.");
        expect(w, "editor", "false");
        expect(w, "memoryTier", "true");
        expect(w, "earlyTier",
               "ReferenceError: Native property getter [SystemInfo::memoryTier] cannot be used in early execution.");
        expect(w, "earlySettings", "ok");
        expect(w, "settings", "{\"test:on\":true,\"test:level\":4}");
        expect(w, "waitZero", "EngineError: ");
        expect(w, "waited", "true");
        expect(w, "restrictedRuleRead", "ok");
        expect(w, "restrictedRuleWrite",
               "ReferenceError: Native property setter [GameRules::pvp] cannot be used in restricted execution.");
        expect(w, "restrictedTime",
               "ReferenceError: Native function [World::setTimeOfDay] cannot be used in restricted execution.");
        expect(w, "restrictedDifficulty",
               "ReferenceError: Native function [World::setDifficulty] cannot be used in restricted execution.");
        expect(w, "restrictedSpawn",
               "ReferenceError: Native function [World::setDefaultSpawnLocation] cannot be used in restricted "
               "execution.");
        expect(w, "pvp", "true");
        expect(w, "rules", "false,3");
        expect(w, "ruleType", "TypeError: Native type conversion failed.");
        expect(w, "time", "229000,13000,9");
        expect(w, "moon", "true");
        expect(w, "timeRange", "Error: ");
        expect(w, "difficulty", "Hard");
        expect(w, "spawn", "10,70,-5");
        expect(w, "spawnBounds", "LocationOutOfWorldBoundariesError: ");
        expect(w, "players", "0,0");
        expect(w, "seed", "string");
        expect(w, "hardcore", "false");
        expect(w, "bytes", "true");
        expect(w, "noNamespace", "NamespaceNameError: ");
        expect(w, "minecraftNamespace", "true,DisallowedNamespace");
        expect(w, "sendEvent", "ok");
        const GameRules &rules = owner.getWorlds().getDefault().getOverworld().getGameRules();
        require(!rules.getBool("pvp") && rules.getInt("randomtickspeed") == 3, "game rules not written");

        const std::unique_ptr<json::Value> components = result(owner, "components");
        const json::Value &c = *components;
        const std::string params = "{\"value\":42,\"list\":[\"a\",true]}";
        expect(c, "noNamespace", "true,NoNamespace");
        expect(c, "minecraftNamespace", "NamespaceNameError: ");
        expect(c, "notObject", "TypeError: Native type conversion failed.");
        expect(c, "registerBlock", "ok");
        expect(c, "duplicate",
               "BlockCustomComponentAlreadyRegisteredError: Block custom component 'test:hooks' was already registered.");
        expect(c, "registerItem", "ok");
        expect(c, "itemDuplicate",
               "ItemCustomComponentAlreadyRegisteredError: Item custom component 'test:wand' was already registered.");
        expect(c, "beforePlace",
               "BlockComponentPlayerPlaceBeforeEvent|cancel+face+permutationToPlace+player|" + params
                       + "|test:block|Up");
        expect(c, "onPlace", "BlockComponentOnPlaceEvent|previousBlock|" + params + "|minecraft:air");
        expect(c, "onPlayerInteract",
               "BlockComponentPlayerInteractEvent|face+faceLocation+player|" + params + "|Up|0.25,0.75|truetrue|4");
        expect(c, "onTick", "BlockComponentTickEvent||" + params);
        expect(c, "onRandomTick", "BlockComponentRandomTickEvent||" + params);
        expect(c, "onStepOn", "BlockComponentStepOnEvent|entity|" + params);
        expect(c, "onStepOff", "BlockComponentStepOffEvent|entity|" + params);
        expect(c, "onEntityFallOn", "BlockComponentEntityFallOnEvent|entity+fallDistance|" + params + "|3.5;");
        expect(c, "onRedstoneUpdate", "2>5;5>9;0>9;0>9;");
        expect(c, "onBreak", "BlockComponentBlockBreakEvent|brokenBlockPermutation+entitySource|" + params + "|true");
        expect(c, "onPlayerBreak", "BlockComponentPlayerBreakEvent|brokenBlockPermutation+player|" + params);
        require(c.get("onPlayerDestroy") == nullptr, "a 2.x pack received the 1.x onPlayerDestroy hook");
        expect(c, "onUse", "ItemComponentUseEvent|itemStack+source|{}");
        expect(c, "onUseOn",
               "ItemComponentUseOnEvent|blockFace+faceLocation+itemStack+source+usedOnBlockPermutation|{}|Up|1|"
               "truetrue");
        expect(c, "onCompleteUse", "ItemComponentCompleteUseEvent|itemStack+source|{}");
        expect(c, "onConsume", "ItemComponentConsumeEvent|itemStack+source|{}");
        // onConsume needs minecraft:food and runs before onCompleteUse
        expect(c, "useOrder", "test:wand:complete;test:snack:consume;test:snack:complete;");
        expect(c, "onHitEntity", "ItemComponentHitEntityEvent|attackingEntity+hadEffect+hitEntity+itemStack|{}");
        expect(c, "onBeforeDurabilityDamage",
               "ItemComponentBeforeDurabilityDamageEvent|attackingEntity+durabilityDamage+hitEntity+itemStack|{}|2");
        expect(c, "onMineBlock", "ItemComponentMineBlockEvent|itemStack+minedBlockPermutation+source|{}");
        expect(c, "litHook", "ok");
        expect(c, "stateHook", "ok");
        expect(c, "resolve", "2,false");
        expect(c, "setPermutation", "ok");
        expect(c, "stateChange", "BlockComponentBlockStateChangeEvent|false|0");
        expect(c, "litTick", "{\"lit\":true}");
        expect(c, "registerClock", "ok");
        expect(c, "dimRegister", "ok");
        expect(c, "dimDuplicate",
               "CustomDimensionAlreadyRegisteredError: Custom dimension 'test:void' was already registered.");
        expect(c, "dimNoNamespace", "NamespaceNameError,NoNamespace");
        expect(c, "dimMinecraft", "NamespaceNameError,DisallowedNamespace");
        expect(c, "dimChars", "CustomDimensionNameError: Custom dimension 'test:a b' has invalid characters.");
        expect(c, "dimLate",
               "CustomDimensionInvalidRegistryError: Cannot register custom dimension 'test:late' outside of "
               "StartupEvent.");
        expect(c, "dimId", "test:void");
        expect(c, "dimSame", "true");
        expect(c, "itemOrder", "durability;hit;");
        expect(c, "clockNames", "no,no,no,no,no,ok");
        expect(c, "clockDuplicate",
               "WorldClockRegistrationError: Failed to register world clock 'test:clock'. World clock name already "
               "exists.");
        expect(c, "clockName",
               "WorldClockRegistrationError: Failed to register world clock 'noname'. World clock has invalid or "
               "missing name.");
        expect(c, "clockPeriod",
               "WorldClockRegistrationError: Failed to register world clock 'test:other'. Time marker period must be "
               "greater than its time.");
        expect(c, "lateClock",
               "WorldClockInvalidRegistryError: Cannot register world clock 'test:late' outside of StartupEvent.");
        expect(c, "clockSame", "true");
        expect(c, "clockMissing", "WorldClockNotFoundError: Failed to find world clock 'test:nope'.");
        expect(c, "overworld",
               "minecraft:day@1000/24000,minecraft:noon@6000/24000,minecraft:sunset@12000/24000,"
               "minecraft:night@13000/24000,minecraft:midnight@18000/24000,minecraft:sunrise@23000/24000");
        expect(c, "clockTime", "13");
        expect(c, "rewind", "ok@12");
        expect(c, "skip", "ok@12");
        expect(c, "setMarker", "ok@2");
        expect(c, "rewindEarly",
               "WorldClockRewindError: Failed to rewind world clock 'test:clock'. Current time is behind time marker "
               "'test:mark'");
        expect(c, "unknownMarker",
               "WorldClockTimeMarkerNotFoundError: Failed to find time marker 'test:unknown' in world clock "
               "'test:clock'.");
        expect(c, "addMarker",
               "ok,WorldClockAddTimeMarkerError: Failed to add time marker 'test:added' on world clock 'test:clock' "
               "Time marker name already exists.");
        expect(c, "removeMarker", "ok,WorldClockInvalidTimeMarkerError: ");
        expect(c, "removeMinecraft",
               "WorldClockRemoveMinecraftTimeMarkerError: Failed to remove time marker 'minecraft:noon' from world "
               "clock 'minecraft:overworld'. A time marker with the 'minecraft' namespace cannot be removed.");
        expect(c, "paused", "test:clock");
        expect(c, "markers", "2;7;12;12;2;2147483647;");
        expect(c, "modified", "4");
        expect(c, "restart", "test:clock|0");
        require(c.get("otherClock") == nullptr, "a clock filter let another clock's event through");

        const std::unique_ptr<json::Value> stable1 = result(owner, "stable1");
        expect(*stable1, "alone", "true");
        expect(*stable1, "gameMode", "survival,undefined");
        expect(*stable1, "worldInitialize", "true");
        expect(*stable1, "worldLoad", "false");
        expect(*stable1, "systemBefore", "false");
        expect(*stable1, "early", "ok");
        expect(*stable1, "startupEvent", "false");
        expect(*stable1, "sbValidMethod", "function,function");
        expect(*stable1, "register", "ok");
        expect(*stable1, "legacyName", "CustomComponentNameError,1");
        expect(*stable1, "legacyInteract", "1|BlockComponentPlayerInteractEvent");
        expect(*stable1, "legacyDestroy", "destroyedBlockPermutation+player");
        require(stable1->get("legacyBreak") == nullptr, "a 1.x pack received the 2.x onPlayerBreak hook");

        const std::unique_ptr<json::Value> beta = result(owner, "beta");
        expect(*beta, "alone", "true");
        expect(*beta, "chatSend", "true");
        expect(*beta, "blockSolid", "true");
        require(BlockSolidity::isSolid("minecraft:stone") && BlockSolidity::isSolid("minecraft:oak_double_slab")
                        && !BlockSolidity::isSolid("minecraft:oak_slab") && !BlockSolidity::isSolid("minecraft:glass")
                        && !BlockSolidity::isSolid("minecraft:air"),
                "the block solidity table disagrees with the game dump");
        expect(*beta, "form", "true");

        const std::unique_ptr<json::Value> custom = result(owner, "commands");
        expect(*custom, "enums", "PlayerSelector,1,Server,EnumDependencyMissing,2");
        expect(*custom, "registry", "true");
        expect(*custom, "enumAdd", "ok");
        expect(*custom, "enumDuplicate",
               "CustomCommandError,AlreadyRegistered,Custom Command Enum 'cmd:color' was already registered.");
        expect(*custom, "enumNoNamespace", "NamespaceNameError,NoNamespace,");
        expect(*custom, "enumMismatch",
               "CustomCommandError,NamespaceMismatch,Custom Command Enum namespaces must match. Namespace 'other' does not "
               "match existing namespace 'cmd'.");
        expect(*custom, "register", "ok");
        expect(*custom, "duplicate",
               "CustomCommandError,AlreadyRegistered,Custom Command 'cmd:echo' was already registered.");
        expect(*custom, "noNamespace", "NamespaceNameError,NoNamespace,");
        expect(*custom, "mismatch",
               "CustomCommandError,NamespaceMismatch,Custom Command Enum namespaces must match. Namespace 'other' does not "
               "match existing namespace 'cmd'.");
        expect(*custom, "missingEnum",
               "CustomCommandError,EnumDependencyMissing,Custom Command depends on one or more unknown enums [cmd:nope].");
        expect(*custom, "badType", "TypeError: Native type conversion failed.");
        expect(*custom, "give", "ok");
        expect(*custom, "limit",
               "CustomCommandError,ParameterLimit,Custom Command 'cmd:nine' has '9' parameters, the limit is '8'.");
        expect(*custom, "eight", "ok");
        expect(*custom, "admin", "ok");
        expect(*custom, "adminDenied",
               "CommandError: "
                       + Localization::getInstance().translate("commands.generic.error.permissions",
                                                               std::vector<std::string>{"cmd:admin"}));
        expect(*custom, "late",
               "CustomCommandError,RegistryInvalid,Custom Command registration failed outside of the ModuleStartup event.");
        expect(*custom, "lateEnum",
               "CustomCommandError,RegistryInvalid,Custom Command Enum registration failed outside of the ModuleStartup "
               "event.");
        expect(*custom, "dimension", "true,1|true,Server,none,none,4,red,1/2/3,undefined,undefined,undefined");
        expect(*custom, "failed", "0");
        expect(*custom, "unknown",
               "CommandError: Unknown command: nosuchcommand. Please check that the command exists "
               "and that you have permission to use it.");
        expect(*custom, "parse", "CommandError: 'x' is not a valid number");
        expect(*custom, "playerRun",
               "1|true,Entity,minecraft:player,none,6,blue,5/6/7,true,minecraft:player,undefined");
        expect(*custom, "restricted", "true");
        expect(*custom, "last", "true,Block,none,block,8,red,undefined,undefined,undefined,undefined");
        expect(*custom, "secret", "true,Server,none,none");
        expect(*custom, "calls", "7");

        const std::unique_ptr<json::Value> betaCommands = result(owner, "betacommands");
        expect(*betaCommands, "named", "ok");
        expect(*betaCommands, "state", "off");
        expect(*betaCommands, "unexpected",
               "CustomCommandError,UnexpectedEnumName,Custom Command parameter 'n' using enumName 'beta:mode' but is not "
               "an Enum type.");

        const auto &properties = owner.getWorldDynamicProperties();
        require(properties.find("undeclared") == properties.end(), "an undeclared module was importable");
        require(properties.find("rejected") == properties.end(), "a pack with an invalid manifest ran");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-scripts-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        // the server writes a restrictive default module list when there is none: these packs use every module
        writeFile(directory / "config" / "default" / "permissions.json", R"({"allowed_modules": ["*"]})");
        // the game's file: tab comments, crlf; haste is left out to check the key comes back
        writeFile(directory / "resource_packs" / "vanilla" / "texts" / "en_US.lang",
                  "## effects\npotion.moveSpeed=Speed\t#\r\npotion.potency.1=II\n");
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script API regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
