#include "Scripting/Binding/ScriptBootstraps.h"

namespace {
    // native classes the game builds itself: a script calling the constructor gets the game's ReferenceError
    const char *ERRORS = R"JS(
(function (names) {
    const token = Symbol();
    const classes = {};
    const named = (cls, name) => {
        Object.defineProperty(cls, "name", { value: name });
        Object.defineProperty(cls.prototype, "name", { value: name, writable: true, configurable: true });
        classes[name] = cls;
    };
    for (const name of names) {
        if (name === "InvalidArgumentError") {
            named(class extends Error {
                constructor(funcName, argTypeName, errorType, argIndex) {
                    super();
                    Object.defineProperty(this, "index", { value: argIndex, enumerable: true });
                    Object.defineProperty(this, "type", { value: errorType, enumerable: true });
                }
            }, name);
            continue;
        }
        named(class extends Error {
            constructor(key, message) {
                if (key !== token)
                    throw new ReferenceError("No constructor for native class '" + name + "'.");
                super(message);
            }
        }, name);
    }
    const make = (name, message, fields) => {
        const cls = classes[name];
        let error;
        if (cls === undefined) {
            error = new Error(message);
        } else if (name === "InvalidArgumentError") {
            error = new cls();
            Object.defineProperty(error, "message", { value: message, writable: true, configurable: true });
        } else {
            error = new cls(token, message);
        }
        if (fields)
            for (const key of Object.keys(fields))
                Object.defineProperty(error, key, { value: fields[key], enumerable: true });
        return error;
    };
    return { classes, make };
})
)JS";

    const char *DATA = R"JS(
(function (native) {
    const token = Symbol();
    const guard = (key, name) => {
        if (key !== token)
            throw new ReferenceError("No constructor for native class '" + name + "'.");
    };
    const normalizeId = (id) => {
        const text = String(id);
        return text.includes(":") ? text : "minecraft:" + text;
    };

    const permutationCache = new Map();
    const permutationsOf = (id) => {
        if (!permutationCache.has(id)) {
            const list = native.permutations(id);
            permutationCache.set(id, list === undefined ? null : list);
        }
        return permutationCache.get(id);
    };

    const infoCache = new Map();
    const infoOf = (id, states) => {
        const key = id + JSON.stringify(states);
        if (!infoCache.has(key))
            infoCache.set(key, native.blockInfo(id, states));
        return infoCache.get(key);
    };
    const water = (liquidType) => {
        if (liquidType !== "Water")
            throw new TypeError("Native type conversion failed.");
    };

    class BlockType {
        #id;
        constructor(key, id) {
            guard(key, "BlockType");
            this.#id = id;
        }
        get id() {
            return this.#id;
        }
        get localizationKey() {
            return infoOf(this.#id, {}).localizationKey;
        }
    }

    class BlockPermutation {
        #type;
        #states;
        constructor(key, id, states) {
            guard(key, "BlockPermutation");
            // the registered type when there is one: permutation.type === BlockTypes.get(id)
            this.#type = allBlockTypes().get(id) || new BlockType(token, id);
            this.#states = states;
        }

        get type() {
            return this.#type;
        }

        getAllStates() {
            return Object.assign({}, this.#states);
        }

        getState(name) {
            return this.#states[name];
        }

        get localizationKey() {
            return infoOf(this.#type.id, this.#states).localizationKey;
        }

        getTags() {
            return infoOf(this.#type.id, this.#states).tags.slice();
        }

        hasTag(tag) {
            return infoOf(this.#type.id, this.#states).tags.includes(String(tag));
        }

        canContainLiquid(liquidType) {
            water(liquidType);
            return infoOf(this.#type.id, this.#states).canContainLiquid;
        }

        canBeDestroyedByLiquidSpread(liquidType) {
            water(liquidType);
            return infoOf(this.#type.id, this.#states).destroyedBySpread;
        }

        liquidSpreadCausesSpawn(liquidType) {
            water(liquidType);
            return infoOf(this.#type.id, this.#states).spawnsOnSpread;
        }

        isLiquidBlocking(liquidType) {
            water(liquidType);
            return infoOf(this.#type.id, this.#states).liquidBlocking;
        }

        getItemStack(amount) {
            return native.permutationItem(this.#type.id, this.#states, amount === undefined ? 1 : amount);
        }

        withState(name, value) {
            return BlockPermutation.resolve(this.#type.id, Object.assign({}, this.#states, { [name]: value }));
        }

        matches(id, states) {
            if (normalizeId(id) !== this.#type.id)
                return false;
            if (states) {
                for (const key of Object.keys(states)) {
                    if (this.#states[key] !== states[key])
                        return false;
                }
            }
            return true;
        }

        static resolve(id, states) {
            const blockId = normalizeId(id);
            const list = permutationsOf(blockId);
            if (!list || list.length === 0)
                throw new Error("Unknown block type " + blockId);

            const wanted = Object.assign({}, list[0]);
            if (states) {
                for (const key of Object.keys(states)) {
                    if (!(key in wanted))
                        throw new Error("Block " + blockId + " has no state " + key);
                    wanted[key] = states[key];
                }
            }

            for (const candidate of list) {
                let same = true;
                for (const key of Object.keys(wanted)) {
                    if (candidate[key] !== wanted[key]) {
                        same = false;
                        break;
                    }
                }
                if (same)
                    return new BlockPermutation(token, blockId, candidate);
            }
            throw new Error("Invalid state value for block " + blockId);
        }
    }

    let blockTypes = null;
    const allBlockTypes = () => {
        if (blockTypes === null) {
            blockTypes = new Map();
            for (const name of native.blockNames())
                blockTypes.set(name, new BlockType(token, name));
        }
        return blockTypes;
    };

    class BlockTypes {
        constructor(key) {
            guard(key, "BlockTypes");
        }
        static get(id) {
            return allBlockTypes().get(normalizeId(id));
        }
        static getAll() {
            return Array.from(allBlockTypes().values());
        }
    }

    const compareStateValues = (left, right) => {
        if (typeof left === typeof right && typeof left !== "string")
            return Number(left) - Number(right);
        return 0;
    };

    class BlockStateType {
        #id;
        #validValues;
        constructor(key, id, validValues) {
            guard(key, "BlockStateType");
            this.#id = id;
            this.#validValues = validValues;
        }
        get id() {
            return this.#id;
        }
        get validValues() {
            return this.#validValues.slice();
        }
    }

    let blockStates = null;
    const allBlockStates = () => {
        if (blockStates === null) {
            const values = new Map();
            for (const name of native.blockNames()) {
                for (const states of permutationsOf(name) || []) {
                    for (const key of Object.keys(states)) {
                        if (!values.has(key))
                            values.set(key, []);
                        const known = values.get(key);
                        if (!known.includes(states[key]))
                            known.push(states[key]);
                    }
                }
            }
            blockStates = new Map();
            for (const [key, known] of values)
                blockStates.set(key, new BlockStateType(token, key, known.slice().sort(compareStateValues)));
        }
        return blockStates;
    };

    class BlockStates {
        constructor(key) {
            guard(key, "BlockStates");
        }
        static get(name) {
            return allBlockStates().get(String(name));
        }
        static getAll() {
            return Array.from(allBlockStates().values());
        }
    }

    class ItemType {
        #id;
        constructor(key, id) {
            guard(key, "ItemType");
            this.#id = id;
        }
        get id() {
            return this.#id;
        }
    }

    let itemTypes = null;
    const allItemTypes = () => {
        if (itemTypes === null) {
            itemTypes = new Map();
            for (const name of native.itemTypeNames())
                itemTypes.set(name, new ItemType(token, name));
        }
        return itemTypes;
    };

    class ItemTypes {
        constructor(key) {
            guard(key, "ItemTypes");
        }
        static get(id) {
            return allItemTypes().get(normalizeId(id));
        }
        static getAll() {
            return Array.from(allItemTypes().values());
        }
    }

    // a job runs its generator on a tick budget, the way the game shares the frame between jobs
    const installJobs = (system) => {
        const JOB_BUDGET_MS = 5;
        const jobs = new Map();
        let nextJobId = 1;

        const finish = (jobId) => {
            const runId = jobs.get(jobId);
            if (runId !== undefined) {
                system.clearRun(runId);
                jobs.delete(jobId);
            }
        };

        Object.defineProperty(system, "runJob", {
            value: (generator) => {
                const jobId = nextJobId++;
                const runId = system.runInterval(() => {
                    const deadline = Date.now() + JOB_BUDGET_MS;
                    try {
                        do {
                            if (generator.next().done) {
                                finish(jobId);
                                return;
                            }
                        } while (Date.now() < deadline);
                    } catch (error) {
                        finish(jobId);
                        throw error;
                    }
                }, 1);
                jobs.set(jobId, runId);
                return jobId;
            }
        });
        Object.defineProperty(system, "clearJob", { value: finish });
    };

    return { BlockPermutation, BlockType, BlockTypes, BlockStateType, BlockStates, ItemType, ItemTypes, installJobs };
})
)JS";

    const char *VOLUMES = R"JS(
(function (native) {
    const token = Symbol();
    const guard = (key, name) => {
        if (key !== token)
            throw new ReferenceError("No constructor for native class '" + name + "'.");
    };
    const vector = (value) => {
        if (value === null || typeof value !== "object")
            throw new TypeError("Native type conversion failed.");
        const x = Number(value.x), y = Number(value.y), z = Number(value.z);
        if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z))
            throw new TypeError("Native type conversion failed.");
        return { x: Math.floor(x), y: Math.floor(y), z: Math.floor(z) };
    };

    class BlockLocationIterator {
        #next;
        constructor(key, next) {
            guard(key, "BlockLocationIterator");
            this.#next = next;
        }
        [Symbol.iterator]() {
            return this;
        }
        next() {
            const value = this.#next();
            return value === undefined ? { done: true, value: undefined } : { done: false, value };
        }
    }

    class BlockVolumeBase {
        constructor(key) {
            guard(key, "BlockVolumeBase");
        }
        getCapacity() {
            const span = this.getSpan();
            return span.x * span.y * span.z;
        }
        getSpan() {
            const min = this.getMin(), max = this.getMax();
            return { x: max.x - min.x + 1, y: max.y - min.y + 1, z: max.z - min.z + 1 };
        }
        // sub_14DD6ADE0, sub_14DD6B520: a bounded min-heap over (key, x, y, z) as floats; a location replaces
        // the least kept one only on a strictly larger key, then all come out from the largest
        #nearest(count, location, sign) {
            const n = Math.max(0, count | 0);
            if (n === 0)
                throw native.error("ArgumentOutOfBoundsError", "", { minValue: 1, maxValue: 4294967295, value: 0 });
            const at = { x: Math.fround(Number(location.x)), y: Math.fround(Number(location.y)),
                         z: Math.fround(Number(location.z)) };
            const f = Math.fround;
            const less = (a, b) => a[0] !== b[0] ? a[0] < b[0] : a[1] !== b[1] ? a[1] < b[1]
                : a[2] !== b[2] ? a[2] < b[2] : a[3] < b[3];
            const kept = [];
            this.forEachLocation((p) => {
                const dx = f(p.x - at.x), dy = f(p.y - at.y), dz = f(p.z - at.z);
                const d = f(f(dz * dz) + f(f(dy * dy) + f(dx * dx)));
                const entry = [sign < 0 ? -d : d, p.x, p.y, p.z];
                if (kept.length < n) {
                    kept.push(entry);
                    return;
                }
                let least = 0;
                for (let i = 1; i < kept.length; ++i)
                    if (less(kept[i], kept[least]))
                        least = i;
                if (entry[0] > kept[least][0])
                    kept[least] = entry;
            });
            return kept.sort((a, b) => (less(a, b) ? 1 : less(b, a) ? -1 : 0)).map((e) => ({ x: e[1], y: e[2], z: e[3] }));
        }
        forEachLocation(visit) {
            for (const location of this.getBlockLocationIterator())
                visit(location);
        }
        getClosest(count, location) {
            return this.#nearest(count, location, -1);
        }
        getFarthest(count, location) {
            return this.#nearest(count, location, 1);
        }
    }

    class BlockVolume extends BlockVolumeBase {
        constructor(from, to) {
            super(token);
            this.from = vector(from);
            this.to = vector(to);
        }
        getMin() {
            const a = vector(this.from), b = vector(this.to);
            return { x: Math.min(a.x, b.x), y: Math.min(a.y, b.y), z: Math.min(a.z, b.z) };
        }
        getMax() {
            const a = vector(this.from), b = vector(this.to);
            return { x: Math.max(a.x, b.x), y: Math.max(a.y, b.y), z: Math.max(a.z, b.z) };
        }
        isInside(location) {
            const at = vector(location), min = this.getMin(), max = this.getMax();
            return at.x >= min.x && at.x <= max.x && at.y >= min.y && at.y <= max.y && at.z >= min.z && at.z <= max.z;
        }
        translate(delta) {
            const d = vector(delta), a = vector(this.from), b = vector(this.to);
            this.from = { x: a.x + d.x, y: a.y + d.y, z: a.z + d.z };
            this.to = { x: b.x + d.x, y: b.y + d.y, z: b.z + d.z };
        }
        doesLocationTouchFaces(pos) {
            const at = vector(pos), min = this.getMin(), max = this.getMax();
            return this.isInside(at) && (at.x === min.x || at.x === max.x || at.y === min.y || at.y === max.y
                || at.z === min.z || at.z === max.z);
        }
        // sub_148F89560: within one block of this volume, apart on one axis at most; from inside, on a face
        doesVolumeTouchFaces(other) {
            const lo = this.getMin(), hi = this.getMax(), mn = other.getMin(), mx = other.getMax();
            let apart = 0, inside = true, onFace = false;
            for (const k of ["x", "y", "z"]) {
                if (mx[k] < lo[k] - 1 || mn[k] > hi[k] + 1)
                    return false;
                if (mx[k] < lo[k] || mn[k] > hi[k])
                    ++apart;
                if (mn[k] < lo[k] || mx[k] > hi[k])
                    inside = false;
                if (mn[k] === lo[k] || mx[k] === hi[k])
                    onFace = true;
            }
            return inside ? onFace : apart <= 1;
        }
        // other inside this one is Contains: sub_148F89760(this, other)
        intersects(other) {
            const a = this.getMin(), b = this.getMax(), c = other.getMin(), d = other.getMax();
            if (c.x > b.x || d.x < a.x || c.y > b.y || d.y < a.y || c.z > b.z || d.z < a.z)
                return 0;
            if (c.x >= a.x && d.x <= b.x && c.y >= a.y && d.y <= b.y && c.z >= a.z && d.z <= b.z)
                return 1;
            return 2;
        }
        // forEach runs x, then z, then y innermost: sub_148F88D80
        forEachLocation(visit) {
            const min = this.getMin(), max = this.getMax();
            for (let x = min.x; x <= max.x; ++x)
                for (let z = min.z; z <= max.z; ++z)
                    for (let y = min.y; y <= max.y; ++y)
                        visit({ x, y, z });
        }
        // x, then z, then y outermost: sub_14314C1B0
        getBlockLocationIterator() {
            const min = this.getMin(), max = this.getMax();
            let x = min.x, y = min.y, z = min.z;
            return new BlockLocationIterator(token, () => {
                if (y > max.y)
                    return undefined;
                const value = { x, y, z };
                if (++x > max.x) {
                    x = min.x;
                    if (++z > max.z) {
                        z = min.z;
                        ++y;
                    }
                }
                return value;
            });
        }
    }

    const keyOf = (v) => v.x + "," + v.y + "," + v.z;

    class ListBlockVolume extends BlockVolumeBase {
        #locations = new Map();
        constructor(locations) {
            super(token);
            this.add(locations);
        }
        add(locations) {
            for (const location of locations) {
                const v = vector(location);
                this.#locations.set(keyOf(v), v);
            }
        }
        remove(locations) {
            for (const location of locations)
                this.#locations.delete(keyOf(vector(location)));
        }
        getCapacity() {
            return this.#locations.size;
        }
        getMin() {
            const all = Array.from(this.#locations.values());
            if (all.length === 0)
                return { x: 0, y: 0, z: 0 };
            return { x: Math.min(...all.map((v) => v.x)), y: Math.min(...all.map((v) => v.y)),
                     z: Math.min(...all.map((v) => v.z)) };
        }
        getMax() {
            const all = Array.from(this.#locations.values());
            if (all.length === 0)
                return { x: 0, y: 0, z: 0 };
            return { x: Math.max(...all.map((v) => v.x)), y: Math.max(...all.map((v) => v.y)),
                     z: Math.max(...all.map((v) => v.z)) };
        }
        isInside(location) {
            return this.#locations.has(keyOf(vector(location)));
        }
        translate(delta) {
            const d = vector(delta);
            const moved = new Map();
            for (const v of this.#locations.values()) {
                const next = { x: v.x + d.x, y: v.y + d.y, z: v.z + d.z };
                moved.set(keyOf(next), next);
            }
            this.#locations = moved;
        }
        getBlockLocationIterator() {
            const values = Array.from(this.#locations.values()).map((v) => ({ x: v.x, y: v.y, z: v.z }));
            let index = 0;
            return new BlockLocationIterator(token, () => (index < values.length ? values[index++] : undefined));
        }
    }

    return { BlockVolumeBase, BlockVolume, ListBlockVolume, BlockLocationIterator };
})
)JS";

    const char *UI = R"JS(
(function (native) {
    const ft = (v) => (v !== null && typeof v === "object") ? (v.rawtext ? v : { rawtext: [v] }) : String(v);

    class ActionFormData {
        #data = { type: "form", title: "", content: "", buttons: [] };
        title(text) {
            this.#data.title = ft(text);
            return this;
        }
        body(text) {
            this.#data.content = ft(text);
            return this;
        }
        button(text, icon) {
            const entry = { text: ft(text) };
            if (icon !== undefined) entry.image = { type: "path", data: String(icon) };
            this.#data.buttons.push(entry);
            return this;
        }
        header(text) {
            return this;
        }
        label(text) {
            return this;
        }
        divider() {
            return this;
        }
        async show(player) {
            const r = await native.showForm(player, JSON.stringify(this.#data));
            if (r.canceled || r.value === null) return { canceled: true, cancelationReason: "UserClosed" };
            return { canceled: false, selection: r.value };
        }
    }

    class MessageFormData {
        #data = { type: "modal", title: "", content: "", button1: "", button2: "" };
        title(text) {
            this.#data.title = ft(text);
            return this;
        }
        body(text) {
            this.#data.content = ft(text);
            return this;
        }
        button1(text) {
            this.#data.button1 = ft(text);
            return this;
        }
        button2(text) {
            this.#data.button2 = ft(text);
            return this;
        }
        async show(player) {
            const r = await native.showForm(player, JSON.stringify(this.#data));
            if (r.canceled || r.value === null) return { canceled: true, cancelationReason: "UserClosed" };
            return { canceled: false, selection: r.value === true ? 0 : 1 };
        }
    }

    class ModalFormData {
        #data = { type: "custom_form", title: "", content: [] };
        title(text) {
            this.#data.title = ft(text);
            return this;
        }
        textField(label, placeholder, defaultValue) {
            this.#data.content.push({ type: "input", text: ft(label), placeholder: String(placeholder || ""), default: defaultValue === undefined ? "" : String(defaultValue) });
            return this;
        }
        toggle(label, defaultValue) {
            const options = (defaultValue !== null && typeof defaultValue === "object") ? defaultValue : { defaultValue };
            this.#data.content.push({ type: "toggle", text: ft(label), default: options.defaultValue === true });
            return this;
        }
        slider(label, min, max, step, defaultValue) {
            let valueStep = 1;
            let def = Number(min);
            if (step !== null && typeof step === "object") {
                if (step.valueStep !== undefined) valueStep = Number(step.valueStep);
                if (step.defaultValue !== undefined) def = Number(step.defaultValue);
            } else {
                if (step !== undefined) valueStep = Number(step);
                if (defaultValue !== undefined) def = Number(defaultValue);
            }
            this.#data.content.push({ type: "slider", text: ft(label), min: Number(min), max: Number(max), step: valueStep, default: def });
            return this;
        }
        dropdown(label, options, defaultIndex) {
            let def = 0;
            if (defaultIndex !== null && typeof defaultIndex === "object") {
                if (defaultIndex.defaultValueIndex !== undefined) def = Number(defaultIndex.defaultValueIndex);
            } else if (defaultIndex !== undefined) {
                def = Number(defaultIndex);
            }
            this.#data.content.push({ type: "dropdown", text: ft(label), options: options.map(ft), default: def });
            return this;
        }
        label(text) { this.#data.content.push({ type: "label", text: ft(text) }); return this; }
        header(text) { this.#data.content.push({ type: "header", text: ft(text) }); return this; }
        divider() { this.#data.content.push({ type: "divider" }); return this; }
        submitButton(text) {
            this.#data.submit = ft(text);
            return this;
        }
        async show(player) {
            const r = await native.showForm(player, JSON.stringify(this.#data));
            if (r.canceled || r.value === null) return { canceled: true, cancelationReason: "UserClosed" };
            return { canceled: false, formValues: r.value };
        }
    }

    return { ActionFormData, MessageFormData, ModalFormData };
})
)JS";

    const char *WORLD = R"JS(
(function (native) {
    const token = Symbol();
    const guard = (key, name) => {
        if (key !== token)
            throw new ReferenceError("No constructor for native class '" + name + "'.");
    };
    const participantKey = (participant) => {
        if (participant === null || participant === undefined) return "";
        if (typeof participant === "string") return participant;
        if (participant.id !== undefined) return String(participant.id);
        if (participant.displayName !== undefined) return String(participant.displayName);
        return String(participant);
    };

    class ScoreboardObjective {
        #id;
        #displayName;
        #scores = new Map();
        constructor(key, id, displayName) {
            guard(key, "ScoreboardObjective");
            this.#id = id;
            this.#displayName = displayName || id;
        }
        get id() {
            return this.#id;
        }
        get displayName() {
            return this.#displayName;
        }
        getScore(participant) {
            const key = participantKey(participant);
            return this.#scores.has(key) ? this.#scores.get(key) : undefined;
        }
        setScore(participant, value) {
            const key = participantKey(participant);
            this.#scores.set(key, value | 0);
            native.setScore(this.#id, key, value | 0);
        }
        addScore(participant, value) {
            const key = participantKey(participant);
            const current = this.#scores.has(key) ? this.#scores.get(key) : 0;
            const next = current + (value | 0);
            this.#scores.set(key, next);
            native.setScore(this.#id, key, next);
            return next;
        }
        removeParticipant(participant) {
            const key = participantKey(participant);
            native.removeScore(this.#id, key);
            return this.#scores.delete(key);
        }
        hasParticipant(participant) {
            return this.#scores.has(participantKey(participant));
        }
        getParticipants() {
            return Array.from(this.#scores.keys()).map((id) => ({ displayName: id, id }));
        }
        getScores() {
            return Array.from(this.#scores.entries()).map(([id, score]) => ({ participant: { displayName: id, id }, score }));
        }
    }

    class Scoreboard {
        #objectives = new Map();
        #displaySlots = new Map();
        constructor(key) {
            guard(key, "Scoreboard");
        }
        addObjective(id, displayName) {
            if (this.#objectives.has(id)) return this.#objectives.get(id);
            const objective = new ScoreboardObjective(token, id, displayName);
            this.#objectives.set(id, objective);
            return objective;
        }
        getObjective(id) {
            return this.#objectives.get(id);
        }
        removeObjective(id) {
            const key = typeof id === "string" ? id : id.id;
            native.removeObjective(key);
            return this.#objectives.delete(key);
        }
        getObjectives() {
            return Array.from(this.#objectives.values());
        }
        getParticipants() {
            const set = new Map();
            for (const objective of this.#objectives.values())
                for (const participant of objective.getParticipants())
                    set.set(participant.id, participant);
            return Array.from(set.values());
        }
        setObjectiveAtDisplaySlot(slot, options) {
            const objective = options && options.objective;
            if (!objective) return undefined;
            native.display(slot, objective.id, objective.displayName || objective.id);
            this.#displaySlots.set(slot, objective);
            return objective;
        }
        clearObjectiveAtDisplaySlot(slot) {
            const previous = this.#displaySlots.get(slot);
            native.clearDisplay(slot);
            this.#displaySlots.delete(slot);
            return previous;
        }
        getObjectiveAtDisplaySlot(slot) {
            const objective = this.#displaySlots.get(slot);
            return objective ? { objective, sortOrder: 0 } : undefined;
        }
    }

    return {
        Scoreboard, ScoreboardObjective, scoreboard: new Scoreboard(token)
    };
})
)JS";
}

namespace ScriptBootstraps {
    const char *errors() {
        return ERRORS;
    }

    const char *data() {
        return DATA;
    }

    const char *ui() {
        return UI;
    }

    const char *volumes() {
        return VOLUMES;
    }

    const char *world() {
        return WORLD;
    }
}
