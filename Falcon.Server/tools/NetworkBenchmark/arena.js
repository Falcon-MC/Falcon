import { world, system, GameMode } from "@minecraft/server";

const GROUP_SIZE = 50;
const GROUP_COUNT = 20;

let dimension;
let ready = false;
let tick = 0;
let hits = 0;
let broken = 0;
let preparation = 0;
let preparing = false;
let measurement;
const blocks = [];

system.afterEvents.scriptEventReceive.subscribe(event => {
    if (event.id === "netbench:start") {
        const now = Date.now();
        measurement = {
            start: now, startTick: system.currentTick, last: now, intervals: [],
            minOnline: world.getAllPlayers().length, hits, broken
        };
        console.warn("NET_MEASURE_START " + JSON.stringify({
            time: now, tick: system.currentTick, online: measurement.minOnline
        }));
    }
    if (event.id === "netbench:stop" && measurement) {
        console.warn("NET_MEASURE_RESULT " + JSON.stringify({
            elapsedMs: Date.now() - measurement.start,
            ticks: system.currentTick - measurement.startTick,
            minOnline: measurement.minOnline, online: world.getAllPlayers().length,
            hits: hits - measurement.hits, broken: broken - measurement.broken,
            intervals: measurement.intervals
        }));
        measurement = undefined;
    }
});

world.afterEvents.entityHurt.subscribe(event => {
    if (event.hurtEntity.typeId === "minecraft:player"
        && event.damageSource.damagingEntity?.typeId === "minecraft:player") {
        hits++;
    }
});
world.afterEvents.playerBreakBlock.subscribe(() => broken++);
world.afterEvents.playerSpawn.subscribe(event => {
    const player = event.player;
    if (!player.name.startsWith("Net")) return;
    const index = Number(player.name.slice(3));
    const group = Math.floor(index / GROUP_SIZE);
    const pair = Math.floor((index % GROUP_SIZE) / 2);
    const x = (group % 5) * 256 + 4 + (pair % 5) * 6 + (index % 2) * 2;
    const z = Math.floor(group / 5) * 256 + 4 + Math.floor(pair / 5) * 4;
    player.teleport({ x: x + 0.5, y: 200, z: z + 0.5 });
    player.setGameMode(GameMode.Survival);
    blocks.push({ x, y: 200, z: z - 1 });
});

system.runInterval(() => {
    try {
        if (!dimension) dimension = world.getDimension("overworld");
        if (!ready) {
            if (!tick) {
                tick = 1;
                dimension.runCommand("gamerule domobspawning false");
                dimension.runCommand("gamerule dotiledrops false");
            }
            const ox = (preparation % 5) * 256;
            const oz = Math.floor(preparation / 5) * 256;
            if (!preparing) {
                dimension.runCommand(`tickingarea add ${ox} 199 ${oz} ${ox + 33} 199 ${oz + 24} prep true`);
                preparing = true;
                return;
            }
            for (let x = ox; x <= ox + 33; x += 16) {
                for (let z = oz; z <= oz + 24; z += 16) {
                    if (!dimension.getBlock({ x, y: 199, z })) return;
                }
            }
            for (let x = ox; x <= ox + 33; x++) {
                for (let z = oz; z <= oz + 24; z++) {
                    dimension.getBlock({ x, y: 199, z }).setType("minecraft:stone");
                }
            }
            dimension.runCommand("tickingarea remove prep");
            console.warn("NET_GROUP_READY " + preparation);
            preparation++;
            preparing = false;
            if (preparation === GROUP_COUNT) {
                ready = true;
                console.warn("NET_ARENA_READY");
            }
            return;
        }
        tick++;
        if (measurement) {
            const now = Date.now();
            measurement.intervals.push(now - measurement.last);
            measurement.last = now;
            if (tick % 20 === 0) {
                measurement.minOnline = Math.min(measurement.minOnline, world.getAllPlayers().length);
            }
        }
        if (tick % 20 === 0) {
            for (const player of world.getAllPlayers()) {
                player.getComponent("minecraft:health").resetToMaxValue();
            }
            for (const position of blocks) {
                const block = dimension.getBlock(position);
                if (block?.typeId === "minecraft:air") block.setType("minecraft:slime");
            }
        }
        if (tick % 100 === 0) {
            console.warn("NET_STATS " + JSON.stringify({ tick, online: world.getAllPlayers().length, hits, broken }));
        }
    } catch (error) {
        console.warn("NET_FAIL " + error);
    }
}, 1);
