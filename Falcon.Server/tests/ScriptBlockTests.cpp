#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/ButtonBlock.h"
#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/Block/LeverBlock.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Actor/ServerActor.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "World/Level/BlockUpdateType.h"
#include "World/Level/Block/Systems/DetectorSystem.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/Structure/McStructure.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <algorithm>
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
                          + "\", \"uuid\": \"00000000-0000-4000-8000-300000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-300000000001\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"2.11.0\"},"
                            " {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    // the fixture: stone floor at y 99, a sign, a cauldron, a jukebox, a lever, a button and a pressure plate
    const char *const BLOCKS = R"JS(
import { world, system, ItemStack, Block, BlockPermutation, BlockVolume, ListBlockVolume, BlockVolumeBase, BlockTypes,
    BlockComponentTypes, BlockSignComponent, BlockVolumeIntersection, LiquidType, BiomeTypes, BiomeType, Structure,
    StructureManager, StructureRotation, StructureMirrorAxis, StructureSaveMode, CloneMode } from "@minecraft/server";

const r = {};
const capture = (f) => {
    try {
        const value = f();
        return value === undefined ? "ok" : String(value);
    } catch (e) {
        return "throw:" + e.name;
    }
};
const log = (line) => { r.events = (r.events || "") + line + ";"; };
const at = (b) => b.x + "," + b.y + "," + b.z;

world.beforeEvents.explosion.subscribe((e) => {
    const blocks = e.getImpactedBlocks();
    log("beforeExplosion:" + (blocks.length > 0) + ":" + e.dimension.id);
    // keep one block only
    e.setImpactedBlocks(blocks.filter((b) => b.x === 20 && b.y === 99 && b.z === 20));
});
world.afterEvents.explosion.subscribe((e) => log("explosion:" + e.getImpactedBlocks().map(at).join("|")));
world.afterEvents.blockExplode.subscribe((e) => log("blockExplode:" + at(e.block) + ":"
    + e.explodedBlockPermutation.type.id));
world.afterEvents.leverAction.subscribe((e) => log("lever:" + at(e.block) + ":" + e.isPowered + ":"
    + (e.player !== undefined)));
world.afterEvents.buttonPush.subscribe((e) => log("button:" + at(e.block) + ":" + (e.source !== undefined)));
world.afterEvents.pressurePlatePush.subscribe((e) => log("platePush:" + e.previousRedstonePower + ">"
    + e.redstonePower + ":" + (e.source !== undefined)));
world.afterEvents.pistonActivate.subscribe((e) => log("piston:" + at(e.block) + ":" + e.isExpanding + ":"
    + (e.piston !== undefined)));
world.afterEvents.tripWireTrip.subscribe((e) => log("trip:" + at(e.block) + ":" + e.isPowered + ":" + e.sources.length));
world.afterEvents.targetBlockHit.subscribe((e) => log("target:" + at(e.block) + ":" + e.previousRedstonePower + ">"
    + e.redstonePower + ":" + (e.source !== undefined)));
world.afterEvents.pressurePlatePop.subscribe((e) => log("platePop:" + e.previousRedstonePower + ">"
    + e.redstonePower));

system.runTimeout(() => {
  try {
    const dim = world.getDimension("overworld");
    const stone = dim.getBlock({ x: 1, y: 99, z: 1 });
    r.type = stone.type.id + ":" + stone.typeId;
    r.permutation = stone.permutation.type.id;
    r.matches = stone.matches("stone") + ":" + stone.matches("minecraft:dirt");
    r.tags = stone.hasTag("stone") + ":" + stone.getTags().includes("stone");
    r.key = stone.localizationKey;
    r.liquid = stone.isLiquid + ":" + stone.isWaterlogged + ":" + stone.canContainLiquid(LiquidType.Water) + ":"
        + stone.isLiquidBlocking("Water") + ":" + stone.liquidCanFlowFromDirection("Water", "Up");
    r.liquidBad = capture(() => stone.canContainLiquid("Lava"));
    r.bottom = JSON.stringify(stone.bottomCenter());
    r.offset = at(stone.offset({ x: 1, y: 0, z: 2 }));
    r.item = stone.getItemStack(3).typeId + "x" + stone.getItemStack(3).amount;
    r.light = typeof stone.getLightLevel() + ":" + typeof stone.getSkyLightLevel();
    r.power = stone.getRedstonePower();
    r.components = stone.getComponents().map((c) => c.typeId).join(",");
    r.movable = stone.getComponent("movable").movementType + ":" + stone.getComponent(BlockComponentTypes.Movable).stickyType;
    r.precipitation = stone.getComponent("precipitation_interactions").obstructsRain();
    r.noPiston = stone.getComponent("piston") === undefined && !stone.hasComponent("minecraft:piston");
    r.componentId = BlockSignComponent.componentId;

    const fence = dim.getBlock({ x: 3, y: 100, z: 3 });
    r.fenceWater = fence.canContainLiquid("Water");
    fence.setWaterlogged(true);
    r.waterlogged = fence.isWaterlogged;
    r.notWaterloggable = capture(() => stone.setWaterlogged(true));

    const permutation = BlockPermutation.resolve("minecraft:stone");
    r.permutationTags = permutation.hasTag("stone") + ":" + permutation.localizationKey + ":"
        + permutation.getItemStack(2).amount + ":" + permutation.canContainLiquid("Water");
    r.typeKey = BlockTypes.get("stone").localizationKey;

    const sign = dim.getBlock({ x: 5, y: 100, z: 5 }).getComponent("sign");
    sign.setText("hello");
    r.sign = (sign instanceof BlockSignComponent) + ":" + sign.getText() + ":" + sign.getRawText().rawtext[0].text
        + ":" + sign.getText("Back") + ":" + sign.getTextDyeColor() + ":" + sign.isWaxed;
    sign.setWaxed(true);
    r.signWaxed = sign.isWaxed + ":" + sign.block.typeId;

    const cauldron = dim.getBlock({ x: 6, y: 100, z: 6 }).getComponent("fluid_container");
    r.cauldron = cauldron.fillLevel + ":" + cauldron.getFluidType();
    cauldron.fillLevel = 6;
    cauldron.setFluidType("Lava");
    r.cauldronAfter = cauldron.fillLevel + ":" + cauldron.getFluidType() + ":" + capture(() => cauldron.setFluidType("Potion"));

    const jukebox = dim.getBlock({ x: 7, y: 100, z: 7 }).getComponent("record_player");
    jukebox.setRecord("minecraft:music_disc_cat");
    r.record = jukebox.getRecord().typeId;
    jukebox.ejectRecord();
    r.recordAfter = jukebox.getRecord() === undefined;

    const color = stone.getComponent("map_color");
    r.mapColor = color.tintMethod + ":" + (color.color.alpha > 0);

    r.height = dim.heightRange.min + ".." + dim.heightRange.max;
    r.dimKey = dim.localizationKey;
    r.loaded = dim.isChunkLoaded({ x: 0, y: 0, z: 0 }) + ":" + dim.isChunkLoaded({ x: 5000, y: 0, z: 5000 });
    r.top = at(dim.getTopmostBlock({ x: 1, z: 1 }));
    r.above = at(dim.getBlockAbove({ x: 3, y: 90, z: 3 }));
    r.below = at(dim.getBlockBelow({ x: 1, y: 105, z: 1 }));
    const ray = dim.getBlockFromRay({ x: 1.5, y: 104, z: 1.5 }, { x: 0, y: -2, z: 0 });
    r.ray = at(ray.block) + ":" + ray.face;
    r.entityRay = dim.getEntitiesFromRay({ x: 0.5, y: 100.5, z: -3 }, { x: 0, y: 0, z: 1 },
        { ignoreBlockCollision: true }).map((h) => h.entity.typeId).join(",");
    r.atLocation = dim.getEntitiesAtBlockLocation({ x: 0, y: 100, z: 0 }).length;
    r.light2 = typeof dim.getLightLevel({ x: 1, y: 100, z: 1 }) + ":" + dim.getSkyLightLevel({ x: 1, y: 100, z: 1 });

    dim.setBlockPermutation({ x: 9, y: 100, z: 9 }, BlockPermutation.resolve("minecraft:dirt"));
    r.setPermutation = dim.getBlock({ x: 9, y: 100, z: 9 }).typeId;

    const volume = new BlockVolume({ x: 10, y: 100, z: 10 }, { x: 11, y: 101, z: 12 });
    r.volume = volume.getCapacity() + ":" + JSON.stringify(volume.getSpan()) + ":" + volume.isInside({ x: 11, y: 101, z: 12 })
        + ":" + volume.doesLocationTouchFaces({ x: 10, y: 100, z: 11 }) + ":" + (volume instanceof BlockVolumeBase);
    r.intersects = volume.intersects(new BlockVolume({ x: 11, y: 100, z: 10 }, { x: 11, y: 100, z: 10 })) + ":"
        + volume.intersects(new BlockVolume({ x: 0, y: 0, z: 0 }, { x: 1, y: 1, z: 1 })) + ":"
        + (BlockVolumeIntersection.Contains === 1);
    const filled = dim.fillBlocks(volume, "minecraft:gold_block");
    r.fill = filled.getCapacity() + ":" + dim.getBlock({ x: 11, y: 101, z: 12 }).typeId + ":" + (filled instanceof ListBlockVolume);
    r.contains = dim.containsBlock(volume, { includeTypes: ["gold_block"] }) + ":"
        + dim.containsBlock(volume, { includeTypes: ["diamond_block"] });
    const list = new ListBlockVolume([{ x: 1, y: 99, z: 1 }, { x: 10, y: 100, z: 10 }]);
    list.add([{ x: 2, y: 99, z: 2 }]);
    list.remove([{ x: 2, y: 99, z: 2 }]);
    r.list = list.getCapacity() + ":" + dim.getBlocks(list, { includeTypes: ["stone"] }).getCapacity();
    r.unloaded = capture(() => dim.fillBlocks(new BlockVolume({ x: 5000, y: 100, z: 5000 }, { x: 5000, y: 100, z: 5000 }), "stone"));
    r.closest = JSON.stringify(volume.getClosest(1, { x: 0, y: 100, z: 10 }));
    let iterated = 0;
    for (const location of volume.getBlockLocationIterator())
        iterated++;
    r.iterated = iterated;

    dim.cloneBlocks({ x: 10, y: 100, z: 10 }, { x: 10, y: 100, z: 10 }, { x: 14, y: 100, z: 14 }, CloneMode.Copy);
    r.clone = dim.getBlock({ x: 14, y: 100, z: 14 }).typeId;
    r.cloneOverlap = capture(() => dim.cloneBlocks({ x: 10, y: 100, z: 10 }, { x: 11, y: 100, z: 10 },
        { x: 11, y: 100, z: 10 }, CloneMode.Copy));

    const biome = dim.getBiome({ x: 0, y: 100, z: 0 });
    r.biome = (biome instanceof BiomeType) + ":" + biome.id.startsWith("minecraft:") + ":" + Array.isArray(biome.getTags());
    r.biomeTypes = (BiomeTypes.getAll().length > 10) + ":" + BiomeTypes.get("plains").id + ":"
        + (BiomeTypes.get("not_a_biome") === undefined);
    r.closestBiome = dim.calculateClosestBiomeFromSeed({ x: 0, y: 64, z: 0 }, "plains") !== undefined;

    r.feature = dim.placeFeature("minecraft:birch_tree_feature", { x: 20, y: 100, z: 25 }) + ":"
        + dim.getBlock({ x: 20, y: 100, z: 25 }).typeId;
    r.badFeature = capture(() => dim.placeFeature("minecraft:nope_feature", { x: 20, y: 100, z: 25 }));
    r.badRule = capture(() => dim.placeFeatureRule("minecraft:nope_rule", { x: 20, y: 100, z: 25 }));

    const manager = world.structureManager;
    r.manager = manager instanceof StructureManager;
    const empty = manager.createEmpty("test:empty", { x: 2, y: 3, z: 4 });
    r.empty = (empty instanceof Structure) + ":" + JSON.stringify(empty.size) + ":"
        + (empty.getBlockPermutation({ x: 0, y: 0, z: 0 }) === undefined) + ":" + empty.isValid;
    r.duplicate = capture(() => manager.createEmpty("test:empty", { x: 1, y: 1, z: 1 }));
    r.outside = capture(() => empty.getBlockPermutation({ x: 5, y: 0, z: 0 }));
    empty.setBlockPermutation({ x: 1, y: 2, z: 3 }, BlockPermutation.resolve("minecraft:dirt"), true);
    r.emptySet = empty.getBlockPermutation({ x: 1, y: 2, z: 3 }).type.id + ":" + empty.getIsWaterlogged({ x: 1, y: 2, z: 3 });

    const taken = manager.createFromWorld("house", dim, { x: 10, y: 100, z: 10 }, { x: 11, y: 101, z: 12 },
        { includeEntities: false });
    r.taken = taken.id + ":" + JSON.stringify(taken.size) + ":" + taken.getBlockPermutation({ x: 1, y: 1, z: 2 }).type.id;
    manager.place(taken, dim, { x: 30, y: 100, z: 30 }, { rotation: StructureRotation.Rotate90 });
    // rotated a quarter turn: size 3 x 2 x 2, the far corner of z lands at x 30
    r.placed = dim.getBlock({ x: 30, y: 101, z: 31 }).typeId + ":" + dim.getBlock({ x: 32, y: 100, z: 30 }).typeId;
    manager.place("mystructure:house", dim, { x: 40, y: 100, z: 40 }, { integrity: 0 });
    r.integrity = dim.getBlock({ x: 40, y: 100, z: 40 }).typeId;
    r.badPlace = capture(() => manager.place("nope", dim, { x: 0, y: 100, z: 0 }));
    taken.saveToWorld();
    r.worldIds = manager.getWorldStructureIds().join(",");
    const copy = taken.saveAs("test:copy");
    r.copy = copy.id + ":" + manager.get("test:copy").id;
    r.packIds = manager.getPackStructureIds().join(",");
    r.packStructure = JSON.stringify(manager.get("test:packed").size);
    r.deleted = manager.delete("test:copy") + ":" + copy.isValid + ":" + capture(() => copy.size) + ":"
        + manager.delete("test:copy");

    r.explosion = dim.createExplosion({ x: 20.5, y: 100, z: 20.5 }, 2, { breaksBlocks: true });
    r.explodedBlock = dim.getBlock({ x: 20, y: 99, z: 20 }).typeId + ":" + dim.getBlock({ x: 21, y: 99, z: 20 }).typeId;
    dim.setWeather("Thunder", 200);
    dim.spawnXp({ x: 0, y: 100, z: 0 }, 5);

    const door = dim.getBlock({ x: 22, y: 101, z: 2 }).getParts();
    r.parts = door.map(at).join("|") + ":" + (stone.getParts() === undefined);
    r.snow = stone.getComponent("precipitation_interactions").accumulatesSnow();

    const fluid = dim.getBlock({ x: 6, y: 100, z: 6 }).getComponent("fluid_container");
    fluid.setFluidType("Water");
    fluid.fluidColor = { red: 1, green: 0, blue: 0, alpha: 1 };
    const fluidColor = fluid.fluidColor;
    fluid.setPotion(new ItemStack("minecraft:potion"));
    r.fluid = fluidColor.red + "," + fluidColor.green + "," + fluidColor.alpha + ":" + fluid.getFluidType() + ":"
        + capture(() => fluid.setPotion(new ItemStack("minecraft:stone")));

    const record = dim.getBlock({ x: 7, y: 100, z: 7 }).getComponent("record_player");
    record.setRecord("minecraft:music_disc_cat");
    const playing = record.isPlaying();
    record.pauseRecord();
    const paused = record.isPlaying();
    record.playRecord();
    r.jukebox = playing + ":" + paused + ":" + record.isPlaying();

    const signBlock = dim.getBlock({ x: 5, y: 100, z: 5 });
    const signComponent = signBlock.getComponent("sign");
    r.dye = capture(() => signComponent.setTextDyeColor("Black")) + ":"
        + capture(() => signComponent.setTextDyeColor("Red")) + ":" + signComponent.getTextDyeColor();
    const store = signBlock.getComponent("dynamic_properties");
    store.set("visits", 3);
    store.set("name", "abc");
    r.dynamic = store.get("visits") + ":" + store.get("name") + ":" + (store.totalByteCount() > 0) + ":"
        + capture(() => store.set("big", "x".repeat(2000))) + ":" + (stone.getComponent("dynamic_properties") === undefined);

    const here = dim.getBiome({ x: 0, y: 100, z: 0 });
    const origin = new BlockVolume({ x: 0, y: 100, z: 0 }, { x: 1, y: 100, z: 1 });
    r.containsBiomes = dim.containsBiomes(origin, { includeBiomes: [here.id] }, true) + ":"
        + dim.containsBiomes(origin, { excludeBiomes: [here.id] }, true) + ":"
        + dim.containsBiomes(origin, { includeBiomes: ["minecraft:not_here"] }, false);

    const herd = manager.createFromWorld("test:herd", dim, { x: 35, y: 100, z: 5 }, { x: 35, y: 101, z: 5 });
    manager.place(herd, dim, { x: 37, y: 100, z: 8 });
    r.herd = dim.getEntities({ type: "minecraft:cow", location: { x: 37.5, y: 100, z: 8.5 }, maxDistance: 1.5 }).length;

    const village = manager.placeJigsawStructure("test:village", dim, { x: 50, y: 100, z: 50 });
    const span = { x: village.max.x - village.min.x + 1, y: village.max.y - village.min.y + 1,
                   z: village.max.z - village.min.z + 1 };
    const pieces = new BlockVolume(village.min, village.max);
    r.jigsaw = (span.x * span.y * span.z) + ":" + dim.containsBlock(pieces, { includeTypes: ["gold_block"] }) + ":"
        + dim.containsBlock(pieces, { includeTypes: ["iron_block"] }) + ":"
        + dim.containsBlock(pieces, { includeTypes: ["diamond_block"] }) + ":"
        + dim.containsBlock(pieces, { includeTypes: ["jigsaw"] });
    const partial = manager.placeJigsaw("test:start_pool", "a", 1, dim, { x: 50, y: 100, z: 58 });
    r.partial = dim.getBlock({ x: 50, y: 100, z: 58 }).typeId + ":" + (partial.min !== undefined);
    r.jigsawErrors = capture(() => manager.placeJigsawStructure("test:nope", dim, { x: 0, y: 100, z: 0 })) + ":"
        + capture(() => manager.placeJigsaw("test:start_pool", "a", 21, dim, { x: 0, y: 100, z: 0 }));
  } catch (e) {
    r.error = e.name + ":" + e.message + ":" + e.stack;
  }
}, 1);
system.runTimeout(() => {
  try {
    const dim = world.getDimension("overworld");
    const lever = dim.getBlock({ x: 12, y: 100, z: 20 }).getComponent("redstone_producer");
    r.leverFaces = lever.getConnectedFaces().length + ":" + (typeof lever.getStronglyPoweredFace());
    r.target = dim.getBlock({ x: 40, y: 100, z: 41 }).getRedstonePower();
  } catch (e) {
    r.error2 = e.name + ":" + e.message;
  }
}, 2);
system.runTimeout(() => world.setDynamicProperty("blocks", JSON.stringify(r)), 3);
)JS";

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

    BlockState vanilla(const char *identifier) {
        const Block *block = VanillaBlocks::fromIdentifier(identifier);
        require(block != nullptr, std::string("no vanilla block ") + identifier);
        return block->toBlockState();
    }

    // two pieces joined by jigsaws: start holds gold and jigsaw a, next holds jigsaw b and iron
    void writeJigsawPack(const std::filesystem::path &pack) {
        const auto jigsaw = [](int facing, const char *name, const char *target, const char *pool, const char *final) {
            BlockState state("minecraft:jigsaw");
            state.mStates.putInt("facing_direction", facing);
            state.mStates.putInt("rotation", 0);
            Tag data = Tag::ofCompound();
            data.putString("id", "JigsawBlock");
            data.putString("name", name);
            data.putString("target", target);
            data.putString("target_pool", pool);
            data.putString("final_state", final);
            return std::make_pair(state, data);
        };
        McStructure start(Vector3i(2, 1, 1));
        const BlockState gold("minecraft:gold_block");
        start.setBlock(Vector3i(0, 0, 0), &gold);
        const auto a = jigsaw(5, "a", "b", "test:pool", "minecraft:diamond_block");
        start.setBlock(Vector3i(1, 0, 0), &a.first);
        start.setBlockEntity(Vector3i(1, 0, 0), a.second);
        McStructure next(Vector3i(2, 1, 1));
        const auto b = jigsaw(4, "b", "", "", "minecraft:iron_block");
        next.setBlock(Vector3i(0, 0, 0), &b.first);
        next.setBlockEntity(Vector3i(0, 0, 0), b.second);
        const BlockState iron("minecraft:iron_block");
        next.setBlock(Vector3i(1, 0, 0), &iron);
        writeFile(pack / "structures" / "test" / "start.mcstructure", start.serialize());
        writeFile(pack / "structures" / "test" / "next.mcstructure", next.serialize());
        writeFile(pack / "worldgen" / "template_pools" / "start.json",
                  R"({"format_version": "1.21.20", "minecraft:template_pool": {
                      // the start piece
                      "description": {"identifier": "test:start_pool"},
                      "elements": [{"element": {"element_type": "minecraft:single_pool_element",
                                                "location": "test:start"}, "weight": 1}],
                      "fallback": "minecraft:empty"}})");
        writeFile(pack / "worldgen" / "template_pools" / "next.json",
                  R"({"format_version": "1.21.20", "minecraft:template_pool": {
                      "description": {"identifier": "test:pool"},
                      "elements": [{"element": {"element_type": "minecraft:single_pool_element",
                                                "location": "test:next"}, "weight": 2}]}})");
        writeFile(pack / "worldgen" / "jigsaw_structures" / "village.json",
                  R"({"format_version": "1.21.20", "minecraft:jigsaw": {
                      "description": {"identifier": "test:village"},
                      "start_pool": "test:start_pool", "max_depth": 2}})");
    }

    // the file format alone: a roundtrip through the bytes and the placement transform
    void checkFormat() {
        McStructure structure(Vector3i(2, 1, 3));
        const BlockState dirt("minecraft:dirt");
        const BlockState water("minecraft:water");
        structure.setBlock(Vector3i(1, 0, 2), &dirt);
        structure.setBlock(Vector3i(1, 0, 2), &water, 1);
        Tag data = Tag::ofCompound();
        data.putString("id", "Sign");
        structure.setBlockEntity(Vector3i(1, 0, 2), data);

        const Tag root = structure.toNbt();
        require(root.getInt("format_version") == 1 && root.get("size")->getList().size() == 3
                        && root.get("structure")->get("block_indices")->getList().size() == 2
                        && root.get("structure")->get("block_indices")->getList()[0].getList().size() == 6,
                "the structure tag does not have the .mcstructure layout");
        // zyx order: x 1, y 0, z 2 is cell (1 * 1 + 0) * 3 + 2
        require(root.get("structure")->get("block_indices")->getList()[0].getList()[5].asInt() == 0
                        && root.get("structure")->get("block_indices")->getList()[0].getList()[0].asInt() == -1,
                "the cells are not in zyx order");
        require(root.get("structure")->get("palette")->get("default")->get("block_position_data")->get("5") != nullptr,
                "the block entity is not keyed by its cell");

        McStructure parsed;
        require(McStructure::parse(structure.serialize(), parsed), "the serialized structure does not parse");
        require(parsed.getSize().x == 2 && parsed.getSize().z == 3 && parsed.getBlock(Vector3i(1, 0, 2)) != nullptr
                        && parsed.getBlock(Vector3i(1, 0, 2))->mName == "minecraft:dirt"
                        && parsed.isWaterlogged(Vector3i(1, 0, 2)) && parsed.getBlock(Vector3i(0, 0, 0)) == nullptr
                        && parsed.getBlockEntity(Vector3i(1, 0, 2)) != nullptr,
                "the roundtrip lost blocks, layers or block entities");
        require(!McStructure::parse("not nbt", parsed), "garbage parsed as a structure");

        McStructure::PlaceOptions options;
        options.mRotation = 1;
        const Vector3i turned = structure.transform(Vector3i(0, 0, 0), options);
        require(turned.x == 2 && turned.z == 0 && structure.rotatedSize(1).x == 3,
                "a quarter turn moved the corner wrong");
        options.mRotation = 0;
        options.mMirror = McStructure::Mirror::X;
        const Vector3i mirrored = structure.transform(Vector3i(0, 0, 0), options);
        require(mirrored.x == 0 && mirrored.z == 2, "the x mirror did not flip z");
    }

    void run(const std::filesystem::path &directory) {
        checkFormat();

        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "blocks", BLOCKS);
        McStructure packed(Vector3i(1, 2, 3));
        writeJigsawPack(packs / "blocks");
        writeFile(packs / "blocks" / "structures" / "test" / "packed.mcstructure", packed.serialize());

        ServerNetworkHandler owner("Script blocks", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the blocks pack did not start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 2001).first->second;
        engine.startup();
        EntityDefinitions::initialize();
        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        for (int32_t chunkX = -1; chunkX <= 3; ++chunkX) {
            for (int32_t chunkZ = -1; chunkZ <= 3; ++chunkZ)
                level.insertChunk(LevelChunk(chunkX, chunkZ));
        }
        const BlockState stone = vanilla("minecraft:stone");
        for (int32_t x = -4; x <= 44; ++x) {
            for (int32_t z = -4; z <= 44; ++z)
                level.setBlock(Vector3i(x, 99, z), stone, false);
        }
        level.setBlock(Vector3i(3, 100, 3), vanilla("minecraft:oak_fence"), false);
        level.setBlock(Vector3i(5, 100, 5), vanilla("minecraft:standing_sign"), false);
        level.getBlockActors().getOrCreate<SignBlockActor>(Vector3i(5, 100, 5));
        level.setBlock(Vector3i(6, 100, 6), CauldronBlock::makeState(CauldronBlock::Liquid::Water, 3), false);
        level.setBlock(Vector3i(7, 100, 7), vanilla("minecraft:jukebox"), false);
        level.getBlockActors().getOrCreate<JukeboxBlockActor>(Vector3i(7, 100, 7));
        level.setBlock(Vector3i(20, 99, 25), vanilla("minecraft:dirt"), false);
        BlockState lower = vanilla("minecraft:wooden_door");
        BlockState upper = lower;
        upper.mStates.putByte("upper_block_bit", 1);
        level.setBlock(Vector3i(22, 100, 2), lower, false);
        level.setBlock(Vector3i(22, 101, 2), BlockState(upper.mName, upper.mStates), false);
        ServerActor *cow = owner.spawnActor(level, "minecraft:cow", Vector3f(35.5f, 100.0f, 5.5f));
        require(cow != nullptr, "no fixture cow");
        // a real database: world structures are written under structuretemplate_
        require(level.openStorage((directory / "worlds").string()), "the test world database did not open");

        player.setName("BlockPlayer");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));

        engine.tick(0);
        engine.tick(1);

        // the lever, button and plate go through the blocks' own handlers
        const Vector3i leverAt(12, 100, 20);
        level.setBlock(leverAt, vanilla("minecraft:lever"), false);
        const BlockState leverState = level.getBlockState(leverAt.x, leverAt.y, leverAt.z);
        LeverBlock(*VanillaBlocks::fromIdentifier("minecraft:lever")).onInteract(owner, player, leverAt, leverState);
        const Vector3i buttonAt(13, 100, 20);
        level.setBlock(buttonAt, vanilla("minecraft:stone_button"), false);
        const BlockState buttonState = level.getBlockState(buttonAt.x, buttonAt.y, buttonAt.z);
        ButtonBlock(*VanillaBlocks::fromIdentifier("minecraft:stone_button"))
                .onInteract(owner, player, buttonAt, buttonState);
        level.setBlock(Vector3i(0, 100, 0), vanilla("minecraft:stone_pressure_plate"), false);
        RedstoneSystem::tick(owner, level);
        // off the plate: the scheduled recheck pops it
        player.setPosition(Vector3f(8.5f, 100.0f, 0.5f));
        RedstoneSystem::onRedstoneUpdate(owner, level, Vector3i(0, 100, 0), level.getBlockState(0, 100, 0),
                                         BlockUpdateType::Scheduled);

        // the piston pushes as soon as a redstone block powers it
        const Vector3i pistonAt(25, 101, 5);
        level.setBlock(pistonAt, vanilla("minecraft:piston"), false);
        level.setBlock(Vector3i(26, 101, 5), vanilla("minecraft:redstone_block"), false);
        PistonSystem::onRedstoneUpdate(owner, level, pistonAt, level.getBlockState(pistonAt.x, pistonAt.y, pistonAt.z));

        // a wire between two hooks facing each other, tripped by the player standing on it
        BlockState eastHook = vanilla("minecraft:tripwire_hook");
        eastHook.mStates.putInt("direction", 3);
        BlockState westHook = vanilla("minecraft:tripwire_hook");
        westHook.mStates.putInt("direction", 1);
        level.setBlock(Vector3i(30, 100, 40), BlockState(eastHook.mName, eastHook.mStates), false);
        level.setBlock(Vector3i(31, 100, 40), vanilla("minecraft:trip_wire"), false);
        level.setBlock(Vector3i(32, 100, 40), vanilla("minecraft:trip_wire"), false);
        level.setBlock(Vector3i(33, 100, 40), BlockState(westHook.mName, westHook.mStates), false);
        player.setPosition(Vector3f(31.5f, 100.0f, 40.5f));
        RedstoneSystem::tick(owner, level);
        const bool hookPowered = level.getBlockState(30, 100, 40).mStates.getBool("powered_bit", false)
                                 && level.getBlockState(33, 100, 40).mStates.getBool("powered_bit", false);
        player.setPosition(Vector3f(8.5f, 100.0f, 0.5f));
        for (int32_t tick = 0; tick < DetectorSystem::WIRE_RECHECK_TICKS; ++tick)
            RedstoneSystem::tick(owner, level);
        const bool hookReleased = !level.getBlockState(30, 100, 40).mStates.getBool("powered_bit", true);

        // a target struck square in the middle of its north face
        level.setBlock(Vector3i(40, 100, 40), vanilla("minecraft:target"), false);
        DetectorSystem::onTargetHit(owner, level, Vector3i(40, 100, 40), Vector3f(40.5f, 100.5f, 40.0f),
                                    Vector3f(40.5f, 100.5f, 39.0f), *cow, true);
        const int edge = DetectorSystem::targetPowerFor(Vector3i(0, 0, 0), Vector3f(0.98f, 0.98f, 0.0f),
                                                        Vector3f(0.98f, 0.98f, -1.0f));
        for (int64_t tick = 2; tick <= 3; ++tick)
            engine.tick(tick);

        const auto found = owner.getWorldDynamicProperties().find("blocks");
        require(found != owner.getWorldDynamicProperties().end(), "no result from the blocks pack");
        const std::unique_ptr<json::Value> parsed = json::parse(found->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the blocks pack wrote no JSON");
        const json::Value &r = *parsed;
        if (r.get("error") != nullptr)
            require(false, "the blocks pack threw: " + text(r, "error"));

        expect(r, "type", "minecraft:stone:minecraft:stone");
        expect(r, "permutation", "minecraft:stone");
        expect(r, "matches", "true:false");
        expect(r, "tags", "true:true");
        expect(r, "key", "tile.stone.name");
        expect(r, "liquid", "false:false:false:true:false");
        expect(r, "liquidBad", "throw:TypeError");
        expect(r, "bottom", "{\"x\":1.5,\"y\":99,\"z\":1.5}");
        expect(r, "offset", "2,99,3");
        expect(r, "item", "minecraft:stonex3");
        expect(r, "light", "number:number");
        expect(r, "power", "0");
        expect(r, "components", "minecraft:map_color,minecraft:movable,minecraft:precipitation_interactions");
        expect(r, "movable", "PushPull:None");
        expect(r, "precipitation", "true");
        expect(r, "noPiston", "true");
        expect(r, "componentId", "minecraft:sign");
        expect(r, "fenceWater", "true");
        expect(r, "waterlogged", "true");
        expect(r, "notWaterloggable", "throw:Error");
        expect(r, "permutationTags", "true:tile.stone.name:2:false");
        expect(r, "typeKey", "tile.stone.name");
        expect(r, "sign", "true:hello:hello::undefined:false");
        expect(r, "signWaxed", "true:minecraft:standing_sign");
        expect(r, "cauldron", "3:Water");
        expect(r, "cauldronAfter", "6:Lava:throw:Error");
        expect(r, "record", "minecraft:music_disc_cat");
        expect(r, "recordAfter", "true");
        expect(r, "mapColor", "None:true");
        expect(r, "height", std::to_string(level.getMinY()) + ".." + std::to_string(level.getMaxY() + 1));
        expect(r, "dimKey", "dimension.dimensionName0");
        expect(r, "loaded", "true:false");
        expect(r, "top", "1,99,1");
        expect(r, "above", "3,99,3");
        expect(r, "below", "1,99,1");
        expect(r, "ray", "1,99,1:Up");
        expect(r, "entityRay", "minecraft:player");
        expect(r, "atLocation", "1");
        expect(r, "light2", "number:" + std::to_string(level.getSkyLightAt(1, 100, 1)));
        expect(r, "setPermutation", "minecraft:dirt");
        expect(r, "volume", "12:{\"x\":2,\"y\":2,\"z\":3}:true:true:true");
        expect(r, "intersects", "1:0:true");
        expect(r, "fill", "12:minecraft:gold_block:true");
        expect(r, "contains", "true:false");
        expect(r, "list", "2:1");
        expect(r, "unloaded", "throw:UnloadedChunksError");
        expect(r, "closest", "[{\"x\":10,\"y\":100,\"z\":10}]");
        expect(r, "iterated", "12");
        expect(r, "clone", "minecraft:gold_block");
        expect(r, "cloneOverlap", "throw:Error");
        expect(r, "biome", "true:true:true");
        expect(r, "biomeTypes", "true:minecraft:plains:true");
        expect(r, "closestBiome", "true");
        expect(r, "badFeature", "throw:InvalidArgumentError");
        expect(r, "badRule", "throw:InvalidArgumentError");
        expect(r, "manager", "true");
        expect(r, "empty", "true:{\"x\":2,\"y\":3,\"z\":4}:true:true");
        expect(r, "duplicate", "throw:Error");
        expect(r, "outside", "throw:ArgumentOutOfBoundsError");
        expect(r, "emptySet", "minecraft:dirt:true");
        expect(r, "taken", "mystructure:house:{\"x\":2,\"y\":2,\"z\":3}:minecraft:gold_block");
        expect(r, "placed", "minecraft:gold_block:minecraft:gold_block");
        expect(r, "integrity", "minecraft:air");
        expect(r, "badPlace", "throw:InvalidStructureError");
        expect(r, "copy", "test:copy:test:copy");
        expect(r, "packIds", "test:next,test:packed,test:start");
        expect(r, "packStructure", "{\"x\":1,\"y\":2,\"z\":3}");
        expect(r, "deleted", "true:false:throw:InvalidStructureError:false");
        expect(r, "explosion", "true");
        expect(r, "explodedBlock", "minecraft:air:minecraft:stone");

        const std::string feature = text(r, "feature");
        require(feature.rfind("true:", 0) == 0, "placeFeature grew no tree: " + feature);

        const std::string events = text(r, "events");
        for (const char *wanted:
             {"beforeExplosion:true:minecraft:overworld;", "blockExplode:20,99,20:minecraft:stone;",
              "explosion:20,99,20;", "lever:12,100,20:true:true;", "button:13,100,20:true;", "platePush:0>"}) {
            require(events.find(wanted) != std::string::npos, std::string("missing event ") + wanted + " in " + events);
        }
        require(level.isThundering() && level.getThunderTime() == 200, "setWeather did not apply");

        if (r.get("error2") != nullptr)
            require(false, "the second blocks run threw: " + text(r, "error2"));
        expect(r, "parts", "22,100,2|22,101,2:true");
        expect(r, "snow", "true");
        expect(r, "fluid", "1,0,1:Potion:throw:Error");
        expect(r, "jukebox", "true:false:true");
        expect(r, "dye", "ok:ok:Red");
        expect(r, "dynamic", "3:abc:true:throw:Error:true");
        expect(r, "containsBiomes", "true:false:false");
        expect(r, "herd", "1");
        expect(r, "jigsaw", "4:true:true:true:false");
        expect(r, "partial", "minecraft:diamond_block:true");
        expect(r, "jigsawErrors", "throw:Error:throw:ArgumentOutOfBoundsError");
        expect(r, "leverFaces", "6:string");
        expect(r, "target", "15");
        require(hookPowered && hookReleased, "the trip wire did not power then release both hooks");
        require(edge == 1, "a hit at the rim did not give the lowest power");
        for (const char *wanted: {"piston:25,101,5:true:true;", "platePop:", "trip:31,100,40:true:1;",
                                  "trip:31,100,40:false:0;", "target:40,100,40:0>15:true;"})
            require(events.find(wanted) != std::string::npos, std::string("missing event ") + wanted + " in " + events);

        // the world copy is in the database, readable by a fresh registry
        Tag saved;
        McStructure reloaded;
        require(level.loadSavedData("structuretemplate_mystructure:house", saved, false)
                        && McStructure::fromNbt(saved, reloaded) && reloaded.getSize().z == 3,
                "saveToWorld did not write the structure to the world database");
        ScriptStructureRegistry fresh;
        fresh.setStorage(&level);
        const std::vector<std::string> ids = fresh.worldIds();
        require(fresh.find("house") != nullptr && std::find(ids.begin(), ids.end(), "mystructure:house") != ids.end(),
                "a fresh registry does not read the world structures back");
        level.closeStorage();
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-blocks-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script block regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
