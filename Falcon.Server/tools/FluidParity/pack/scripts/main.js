import { system, world, BlockPermutation } from "@minecraft/server";

let job = null;

function cell(b) {
  if (!b) return null;
  const id = b.typeId.replace("minecraft:", "");
  let s = id;
  if (id === "air") s = null;
  const d = b.permutation.getState("liquid_depth");
  if (s !== null && d !== undefined) s += ":" + d;
  let w = false;
  try { w = b.isWaterlogged; } catch (e) {}
  if (w) s = (s === null ? "air" : s) + "+w";
  return s;
}

function dump() {
  const dim = world.getDimension(job.dim);
  const [x0, y0, z0, x1, y1, z1] = job.box;
  const out = [];
  for (let y = y0; y <= y1; y++)
    for (let z = z0; z <= z1; z++)
      for (let x = x0; x <= x1; x++) {
        const s = cell(dim.getBlock({ x, y, z }));
        if (s !== null) out.push((x - x0) + "," + (y - y0) + "," + (z - z0) + "=" + s);
      }
  return out.join(" ");
}

system.afterEvents.scriptEventReceive.subscribe((ev) => {
  if (ev.id !== "fl:run") return;
  const p = ev.message.trim().split(/\s+/);
  job = { name: p[0], left: parseInt(p[1]), box: p.slice(2, 8).map((v) => parseInt(v)), dim: p[8], t: 0, last: null, cmds: p.slice(9).join(" ").split(";").map((c) => c.trim()).filter((c) => c.length > 0), started: false, done: new Set() };
  console.warn("FL BEGIN " + job.name + " tick=" + system.currentTick);
});

system.runInterval(() => {
  if (job === null) return;
  // the game tick, not a callback count: callbacks are skipped when the server falls behind
  if (job.start === undefined) job.start = system.currentTick;
  job.t = system.currentTick - job.start;
  {
    const dim = world.getDimension(job.dim);
    for (const raw of job.cmds) {
      let at = 0;
      let c = raw;
      const m = /^@(\d+)\s+(.*)$/.exec(raw);
      if (m) { at = parseInt(m[1]); c = m[2]; }
      if (at > job.t || job.done.has(raw)) continue;
      job.done.add(raw);
      try {
        const w = /^waterlog (-?\d+) (-?\d+) (-?\d+)$/.exec(c);
        if (w) dim.getBlock({ x: parseInt(w[1]), y: parseInt(w[2]), z: parseInt(w[3]) }).setWaterlogged(true);
        else dim.runCommand(c);
      } catch (e) { console.warn("FL ERR " + c + " " + e); }
    }
  }
  const s = dump();
  if (s !== job.last) {
    console.warn("FL T " + job.t + " " + s);
    job.last = s;
  }
  if (job.t + 1 >= job.left) {
    console.warn("FL END " + job.name);
    job = null;
  }
}, 1);

import { BlockTypes, LiquidType, Direction } from "@minecraft/server";

function* propsJob() {
  const dim = world.getDimension("overworld");
  const pos = { x: 40, y: 250, z: 40 };
  const types = BlockTypes.getAll().map((t) => t.id).sort();
  const dirs = [Direction.Down, Direction.Up, Direction.North, Direction.South, Direction.West, Direction.East];
  for (const id of types) {
    let line = id;
    try {
      dim.setBlockType(pos, id);
      const b = dim.getBlock(pos);
      const f = [];
      f.push(b.typeId === id ? "ok" : "as=" + b.typeId);
      try { f.push("contain=" + (b.canContainLiquid(LiquidType.Water) ? 1 : 0)); } catch (e) { f.push("contain=?"); }
      try { f.push("blocking=" + (b.isLiquidBlocking(LiquidType.Water) ? 1 : 0)); } catch (e) { f.push("blocking=?"); }
      try { f.push("spawn=" + (b.liquidSpreadCausesSpawn(LiquidType.Water) ? 1 : 0)); } catch (e) { f.push("spawn=?"); }
      try { f.push("flow=" + dirs.map((d) => (b.liquidCanFlowFromDirection(LiquidType.Water, d) ? 1 : 0)).join("")); } catch (e) { f.push("flow=?"); }
      try { f.push("solid=" + (b.isSolid ? 1 : 0)); } catch (e) {}
      try { f.push("air=" + (b.isAir ? 1 : 0) + " liquid=" + (b.isLiquid ? 1 : 0)); } catch (e) {}
      line += " " + f.join(" ");
    } catch (e) {
      line += " ERR " + e;
    }
    console.warn("FP " + line);
    yield;
  }
  dim.setBlockType(pos, "minecraft:air");
  console.warn("FP END");
}

system.afterEvents.scriptEventReceive.subscribe((ev) => {
  if (ev.id === "fl:props") system.runJob(propsJob());
});

function* gridJob(liquid, y0, wait) {
  const dim = world.getDimension("overworld");
  const types = BlockTypes.getAll().map((t) => t.id).sort();
  const cols = 40;
  const cells = [];
  types.forEach((id, i) => {
    const cx = 100 + (i % cols) * 3;
    const cz = 100 + Math.floor(i / cols) * 2;
    cells.push({ id, cx, cz });
  });
  const x1 = 100 + cols * 3, z1 = 100 + Math.ceil(types.length / cols) * 2;
  for (let x = 98; x <= x1 + 1; x += 16)
    for (let z = 98; z <= z1 + 1; z += 16) {
      dim.runCommand(`fill ${x} ${y0 - 1} ${z} ${Math.min(x + 15, x1 + 1)} ${y0 + 1} ${Math.min(z + 15, z1 + 1)} stone`);
      yield;
    }
  for (const c of cells) {
    try { dim.setBlockType({ x: c.cx + 1, y: y0, z: c.cz }, c.id); } catch (e) {}
    yield;
  }
  for (const c of cells) {
    const b = dim.getBlock({ x: c.cx + 1, y: y0, z: c.cz });
    c.placed = b.typeId;
    dim.setBlockType({ x: c.cx, y: y0, z: c.cz }, liquid === "lava" ? "minecraft:flowing_lava" : "minecraft:flowing_water");
  }
  const start = system.currentTick;
  while (system.currentTick - start < wait) yield;
  for (const c of cells) {
    const b = dim.getBlock({ x: c.cx + 1, y: y0, z: c.cz });
    let w = false;
    try { w = b.isWaterlogged; } catch (e) {}
    const d = b.permutation.getState("liquid_depth");
    console.warn("FG " + liquid + " " + c.id + " placed=" + c.placed + " now=" + b.typeId + (d !== undefined ? ":" + d : "") + (w ? "+w" : ""));
    yield;
  }
  console.warn("FG END " + liquid);
}

system.afterEvents.scriptEventReceive.subscribe((ev) => {
  if (ev.id === "fl:grid") {
    const p = ev.message.trim().split(/\s+/);
    system.runJob(gridJob(p[0], parseInt(p[1]), parseInt(p[2])));
  }
});

function* solidJob(y0, wait) {
  const dim = world.getDimension("overworld");
  const types = BlockTypes.getAll().map((t) => t.id).sort();
  const cols = 34;
  const cells = types.map((id, i) => ({ id, cx: 100 + (i % cols) * 4, cz: 100 + Math.floor(i / cols) * 2 }));
  const x1 = 100 + cols * 4, z1 = 100 + Math.ceil(types.length / cols) * 2;
  for (let x = 98; x <= x1 + 1; x += 16)
    for (let z = 98; z <= z1 + 1; z += 16) {
      dim.runCommand(`fill ${x} ${y0 - 2} ${z} ${Math.min(x + 15, x1 + 1)} ${y0 + 1} ${Math.min(z + 15, z1 + 1)} stone`);
      yield;
    }
  for (const c of cells) {
    try { dim.setBlockType({ x: c.cx + 1, y: y0 - 1, z: c.cz }, c.id); } catch (e) {}
    dim.setBlockType({ x: c.cx + 1, y: y0, z: c.cz }, "minecraft:air");
    yield;
  }
  for (const c of cells) {
    c.placed = dim.getBlock({ x: c.cx + 1, y: y0 - 1, z: c.cz }).typeId;
    dim.setBlockType({ x: c.cx, y: y0, z: c.cz }, "minecraft:flowing_water");
    dim.setBlockType({ x: c.cx + 2, y: y0, z: c.cz }, "minecraft:flowing_water");
  }
  const start = system.currentTick;
  while (system.currentTick - start < wait) yield;
  for (const c of cells) {
    const t = dim.getBlock({ x: c.cx + 1, y: y0, z: c.cz });
    const b = dim.getBlock({ x: c.cx + 1, y: y0 - 1, z: c.cz });
    const d = t.permutation.getState("liquid_depth");
    console.warn("FS " + c.id + " placed=" + c.placed + " t=" + t.typeId + (d !== undefined ? ":" + d : "") + " b=" + b.typeId);
    yield;
  }
  console.warn("FS END");
}

system.afterEvents.scriptEventReceive.subscribe((ev) => {
  if (ev.id === "fl:solid") {
    const p = ev.message.trim().split(/\s+/);
    system.runJob(solidJob(parseInt(p[0]), parseInt(p[1])));
  }
});

system.afterEvents.scriptEventReceive.subscribe((ev) => {
  if (ev.id !== "fl:bench") return;
  const dim = world.getDimension("overworld");
  const phase = ev.message.trim();
  if (phase === "setup") {
    for (let x = 8; x < 88; x += 16)
      for (let z = 8; z < 88; z += 16) {
        dim.runCommand(`fill ${x} 199 ${z} ${Math.min(x + 15, 87)} 199 ${Math.min(z + 15, 87)} stone`);
        dim.runCommand(`fill ${x} 200 ${z} ${Math.min(x + 15, 87)} 230 ${Math.min(z + 15, 87)} air`);
      }
  } else {
    const y = phase === "waterfall" ? 250 : 200;
    const block = phase === "drain" ? "minecraft:air" : "minecraft:flowing_water";
    for (let x = 12; x < 88; x += 8)
      for (let z = 12; z < 88; z += 8)
        dim.setBlockType({ x, y, z }, block);
  }
  console.warn("FB " + phase + " " + system.currentTick);
});
