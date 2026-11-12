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
    const token = Symbol();
    const sealed = (name) => {
        throw new ReferenceError("No constructor for native class '" + name + "'.");
    };
    const field = (target, key, value) => Object.defineProperty(target, key, { value, enumerable: true });
    // sub_14B6271B0
    const REJECT_MESSAGES = {
        ServerShutdown: "Server is shutting down.",
        PlayerQuit: "Player quit before responding.",
        MalformedResponse: "Response from the client was malformed.",
    };
    const reject = (reason) => native.errors.make("FormRejectError", REJECT_MESSAGES[reason], { reason });

    class FormResponse {
        constructor(key, canceled, reason) {
            if (key !== token) sealed(new.target === undefined ? "FormResponse" : new.target.name);
            // 1.0.0 responses carry neither
            if (native.has("FormResponse.canceled")) field(this, "canceled", canceled);
            if (reason !== undefined && native.has("FormResponse.cancelationReason")) field(this, "cancelationReason", reason);
        }
    }
    class ActionFormResponse extends FormResponse {
        constructor(key, canceled, reason, selection) {
            super(key, canceled, reason);
            if (selection !== undefined) field(this, "selection", selection);
        }
    }
    class MessageFormResponse extends FormResponse {
        constructor(key, canceled, reason, selection) {
            super(key, canceled, reason);
            if (selection !== undefined) field(this, "selection", selection);
        }
    }
    class ModalFormResponse extends FormResponse {
        constructor(key, canceled, reason, formValues) {
            super(key, canceled, reason);
            if (formValues !== undefined) field(this, "formValues", formValues);
        }
    }

    // UserClosed and UserBusy cancel; a player leaving or the server stopping rejects
    const settle = async (player, data, Response, read) => {
        const r = await native.showForm(player, JSON.stringify(data));
        if (r.status === "UserClosed" || r.status === "UserBusy")
            return new Response(token, true, r.status);
        if (r.status !== "Answered")
            throw reject(r.status);
        const value = read(r.value);
        if (value === undefined)
            throw reject("MalformedResponse");
        return new Response(token, false, undefined, value);
    };

    // 26.60 action forms list buttons and text in one elements array
    class ActionFormData {
        #data = { type: "form", title: "", content: "", elements: [] };
        title(text) {
            this.#data.title = ft(text);
            return this;
        }
        body(text) {
            this.#data.content = ft(text);
            return this;
        }
        button(text, icon) {
            const entry = { type: "button", text: ft(text) };
            if (icon !== undefined) entry.image = { type: "path", data: String(icon) };
            this.#data.elements.push(entry);
            return this;
        }
        header(text) {
            this.#data.elements.push({ type: "header", text: ft(text) });
            return this;
        }
        label(text) {
            this.#data.elements.push({ type: "label", text: ft(text) });
            return this;
        }
        divider() {
            this.#data.elements.push({ type: "divider", text: "" });
            return this;
        }
        show(player) {
            // sub_143964F00: any integer, taken as the client sends it
            return settle(player, this.#data, ActionFormResponse, (v) => Number.isInteger(v) ? v : undefined);
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
        show(player) {
            return settle(player, this.#data, MessageFormResponse, (v) => {
                if (typeof v === "boolean") return v ? 0 : 1;
                return v === 0 || v === 1 ? v : undefined;
            });
        }
    }

    const options = (value, key) => (value !== null && typeof value === "object") ? value : { [key]: value };
    // 1.x: positional defaults; 2.0.0 moved them into options and added label
    const legacy = !native.has("ModalFormData.label");
    const tooltip = (entry, source) => {
        if (source.tooltip !== undefined) entry.tooltip = ft(source.tooltip);
        return entry;
    };

    class ModalFormData {
        #data = { type: "custom_form", title: "", content: [] };
        title(text) {
            this.#data.title = ft(text);
            return this;
        }
        textField(label, placeholder, textFieldOptions) {
            // 1.x takes the default text itself, a raw message included
            const o = legacy ? { defaultValue: textFieldOptions } : options(textFieldOptions, "defaultValue");
            const def = o.defaultValue === undefined ? "" : (typeof o.defaultValue === "object" ? ft(o.defaultValue) : String(o.defaultValue));
            this.#data.content.push(tooltip({ type: "input", text: ft(label), placeholder: placeholder === undefined ? "" : ft(placeholder), default: def }, o));
            return this;
        }
        toggle(label, toggleOptions) {
            const o = options(toggleOptions, "defaultValue");
            this.#data.content.push(tooltip({ type: "toggle", text: ft(label), default: o.defaultValue === true }, o));
            return this;
        }
        slider(label, min, max, sliderOptions, defaultValue) {
            let o = {};
            if (sliderOptions !== null && typeof sliderOptions === "object") o = sliderOptions;
            else o = { valueStep: sliderOptions, defaultValue };
            const step = o.valueStep === undefined ? 1 : Number(o.valueStep);
            const def = o.defaultValue === undefined ? Number(min) : Number(o.defaultValue);
            this.#data.content.push(tooltip({ type: "slider", text: ft(label), min: Number(min), max: Number(max), step, default: def }, o));
            return this;
        }
        dropdown(label, items, dropdownOptions) {
            const o = options(dropdownOptions, "defaultValueIndex");
            const def = o.defaultValueIndex === undefined ? 0 : Number(o.defaultValueIndex);
            this.#data.content.push(tooltip({ type: "dropdown", text: ft(label), options: items.map(ft), default: def }, o));
            return this;
        }
        label(text) {
            this.#data.content.push({ type: "label", text: ft(text) });
            return this;
        }
        header(text) {
            this.#data.content.push({ type: "header", text: ft(text) });
            return this;
        }
        divider() {
            this.#data.content.push({ type: "divider", text: "" });
            return this;
        }
        submitButton(text) {
            this.#data.submit = ft(text);
            return this;
        }
        show(player) {
            // each value is a bool, a number kept as a float, a string or null; anything else rejects the response
            return settle(player, this.#data, ModalFormResponse, (v) => {
                if (!Array.isArray(v)) return undefined;
                const values = [];
                for (const x of v) {
                    if (x === null) values.push(undefined);
                    else if (typeof x === "number") values.push(Math.fround(x));
                    else if (typeof x === "boolean" || typeof x === "string") values.push(x);
                    else return undefined;
                }
                return values;
            });
        }
    }

    // DDUI: the screen binds to custom_form_data_{id} or message_box_data_{id} in the minecraft store
    const conversion = () => new TypeError("Native type conversion failed.");
    const invalidPlayer = (player, member) => {
        const fields = { type: "minecraft:player" };
        try {
            fields.id = player.id;
        } catch (error) {
            fields.id = "";
        }
        return native.errors.make("InvalidEntityError",
            "Failed to call function '" + member + "' due to Entity being invalid (has the Entity been removed?).", fields);
    };
    const visibility = (formId, reason) => native.errors.make("FormVisibilityError", reason === "AlreadyShowing"
        ? "'" + formId + "' is already showing." : "'" + formId + "' cannot be closed because it is not showing.",
    { formId, reason });

    const observed = new WeakMap();
    const isObservable = (value, cls) => value instanceof cls && observed.has(value);
    // a client write skips the form it came from: the client already shows it
    const publish = (observable, data, source) => {
        const state = observed.get(observable);
        state.data = data;
        for (const callback of state.callbacks.slice())
            callback(data);
        // a copy: a resync re-registers its owner while this loop runs
        for (const owner of [...state.owners])
            if (owner !== source) owner();
    };
    const makeObservable = (name, check) => {
        const cls = class {
            constructor(data, options) {
                if (!check(data)) throw conversion();
                const writable = options !== undefined && options !== null && options.clientWritable === true;
                observed.set(this, { data, writable, callbacks: [], owners: new Set() });
            }
            getData() {
                return observed.get(this).data;
            }
            setData(data) {
                if (!check(data)) throw conversion();
                publish(this, data, undefined);
            }
            subscribe(callback) {
                if (typeof callback !== "function") throw conversion();
                observed.get(this).callbacks.push(callback);
                return callback;
            }
            unsubscribe(callback) {
                const callbacks = observed.get(this).callbacks;
                const index = callbacks.indexOf(callback);
                if (index < 0) return false;
                callbacks.splice(index, 1);
                return true;
            }
        };
        Object.defineProperty(cls, "name", { value: name });
        return cls;
    };
    const isRawMessage = (value) => value !== null && typeof value === "object" && !Array.isArray(value);
    const ObservableBoolean = makeObservable("ObservableBoolean", (v) => typeof v === "boolean");
    const ObservableNumber = makeObservable("ObservableNumber", (v) => typeof v === "number");
    const ObservableUIRawMessage = makeObservable("ObservableUIRawMessage", isRawMessage);
    const ObservableStringBase = makeObservable("ObservableString", (v) => typeof v === "string");
    class ObservableString extends ObservableStringBase {
        // DDUITextFiltering.cpp: the text as it is now, filtered for that player
        getFilteredText(player) {
            const filtered = native.filterText(player, observed.get(this).data);
            if (filtered === undefined) throw invalidPlayer(player, "getFilteredText");
            return filtered;
        }
    }

    // the values a builder reads, and the observables it watches while the form shows
    class Reader {
        constructor() {
            this.watched = new Set();
        }
        read(value) {
            this.watched.add(value);
            return observed.get(value).data;
        }
        text(value) {
            if (isObservable(value, ObservableString) || isObservable(value, ObservableUIRawMessage))
                return this.read(value);
            if (typeof value === "string" || isRawMessage(value)) return value;
            throw conversion();
        }
        flag(value, fallback) {
            if (value === undefined) return fallback;
            if (isObservable(value, ObservableBoolean)) return this.read(value);
            if (typeof value === "boolean") return value;
            throw conversion();
        }
        number(value) {
            if (isObservable(value, ObservableNumber)) return this.read(value);
            if (typeof value === "number") return value;
            throw conversion();
        }
        string(value) {
            if (isObservable(value, ObservableString)) return this.read(value);
            if (typeof value === "string") return value;
            throw conversion();
        }
    }
    const optionsOf = (options) => (options === undefined || options === null) ? {} : options;
    // lists are objects keyed by decimal index with a length, as the game writes the layout
    const list = (entries) => {
        const out = { length: BigInt(entries.length) };
        entries.forEach((entry, index) => {
            out[String(index)] = entry;
        });
        return out;
    };
    const optional = (entry, key, value) => {
        if (value !== undefined) entry[key] = value;
        return entry;
    };
    const imageOf = (reader, entry, details) => {
        if (details === undefined) return entry;
        optional(entry, "image_src", reader.string(details.imageSrc));
        return optional(entry, "image_pack_id", reader.string(details.imagePackId));
    };
    const tooltipOf = (reader, entry, tooltip) => tooltip === undefined ? entry : optional(entry, "tooltip", reader.text(tooltip));
    const descriptionOf = (reader, entry, description) =>
        description === undefined ? entry : optional(entry, "description", reader.text(description));
    // sub_14B660830: a button in the layout or a row
    const layoutButton = (reader, label, options) => {
        const o = optionsOf(options);
        const entry = { button_visible: true, label: reader.text(label), onClick: 0, visible: reader.flag(o.visible, true),
            disabled: reader.flag(o.disabled, false) };
        tooltipOf(reader, entry, o.tooltip);
        return imageOf(reader, entry, o.imageDetails);
    };
    const writableObservable = (value, cls, name) => {
        if (!isObservable(value, cls)) throw conversion();
        if (!observed.get(value).writable)
            throw native.errors.make("InvalidObservableError", "Expect '" + name + "' observable to be client writable.");
    };

    const CLOSE_REASONS = {
        ProgrammaticClose: "ServerClosed",
        ProgrammaticCloseAll: "ServerClosed",
        ClientCanceled: "ClientClosed",
        UserBusy: "UserBusy",
    };

    // the show, close and visibility rules both screens share
    class Screen {
        #player;
        #screenId;
        #propertyPrefix;
        #build;
        #id = 0;
        #showing = false;
        #handlers = new Map();
        #watched = new Set();
        #resync = () => this.#push();
        constructor(player, screenId, propertyPrefix, build) {
            this.#player = player;
            this.#screenId = screenId;
            this.#propertyPrefix = propertyPrefix;
            this.#build = build;
        }
        guard() {
            if (this.#showing)
                throw native.errors.make("InvalidFormModificationError",
                    "'" + this.#screenId + "' cannot be modified because it is already showing.", { formId: this.#screenId });
        }
        showing() {
            return this.#showing;
        }
        #render() {
            const reader = new Reader();
            const handlers = new Map();
            const writable = [];
            const value = this.#build(reader, (path, handler) => {
                writable.push(path);
                handlers.set(path, handler);
            });
            for (const observable of this.#watched)
                observed.get(observable).owners.delete(this.#resync);
            this.#watched = reader.watched;
            for (const observable of this.#watched)
                observed.get(observable).owners.add(this.#resync);
            this.#handlers = handlers;
            return { value, writable };
        }
        #push() {
            if (!this.#showing) return;
            const rendered = this.#render();
            native.updateScreen(this.#player, this.#id, rendered.value, rendered.writable);
        }
        #release() {
            this.#showing = false;
            for (const observable of this.#watched)
                observed.get(observable).owners.delete(this.#resync);
            this.#watched = new Set();
        }
        // settle maps the native outcome: reason for a close, or undefined after a rejection
        show(settle) {
            if (!native.isPlayer(this.#player)) throw invalidPlayer(this.#player, "show");
            if (this.#showing) throw visibility(this.#screenId, "AlreadyShowing");
            const rendered = this.#render();
            this.#showing = true;
            return new Promise((resolve, reject) => {
                const onWrite = (path, value) => {
                    const handler = this.#handlers.get(path);
                    if (handler !== undefined) handler(value, this.#resync);
                };
                const onClose = (outcome) => {
                    this.#release();
                    if (outcome === "InvalidForm")
                        reject(native.errors.make("InvalidFormError", "'" + this.#screenId + "' is not valid and could not be shown.",
                            { formId: this.#screenId }));
                    else if (outcome === "PlayerLeft")
                        reject(native.errors.make("PlayerLeftError", "'" + this.#screenId + "' was rejected because the player left.",
                            { formId: this.#screenId }));
                    else
                        resolve(settle(CLOSE_REASONS[outcome]));
                };
                this.#id = native.showScreen(this.#player, this.#screenId, this.#propertyPrefix, rendered.value,
                    rendered.writable, onWrite, onClose);
                if (this.#id === 0) onClose("PlayerLeft");
            });
        }
        close() {
            if (!native.isPlayer(this.#player)) throw invalidPlayer(this.#player, "close");
            if (!this.#showing) throw visibility(this.#screenId, "NotShowing");
            native.closeScreen(this.#player, this.#id);
        }
    }
    const action = (callback) => (value, source) => callback();
    const binding = (observable) => (value, source) => publish(observable, value, source);

    class CustomForm {
        #screen;
        #title;
        #elements = [];
        #closeButton = false;
        constructor(player, title, options) {
            if (!native.isPlayer(player)) throw invalidPlayer(player, "constructor");
            if (typeof title !== "string" && !isRawMessage(title)) throw conversion();
            this.#title = title;
            this.#screen = new Screen(player, "minecraft:custom_form", "custom_form_data_", (reader, writable) => {
                const layout = this.#elements.map((build, index) => build(reader, "layout[" + index + "]", writable));
                writable("closeButton.onClick", () => {});
                return {
                    title: reader.text(this.#title),
                    closeButton: { button_visible: true, label: "Close", onClick: 0, visible: this.#closeButton },
                    layout: list(layout),
                };
            });
        }
        #add(build) {
            this.#elements.push(build);
            return this;
        }
        button(label, onClick, options) {
            this.#screen.guard();
            if (typeof onClick !== "function") throw conversion();
            return this.#add((reader, path, writable) => {
                writable(path + ".onClick", action(onClick));
                return layoutButton(reader, label, options);
            });
        }
        close() {
            this.#screen.close();
        }
        closeButton() {
            this.#screen.guard();
            this.#closeButton = true;
            return this;
        }
        divider(options) {
            this.#screen.guard();
            const o = optionsOf(options);
            return this.#add((reader) => ({ divider_visible: true, visible: reader.flag(o.visible, true) }));
        }
        dropdown(label, value, items, options) {
            this.#screen.guard();
            writableObservable(value, ObservableNumber, "dropdown value");
            if (!Array.isArray(items)) throw conversion();
            const o = optionsOf(options);
            return this.#add((reader, path, writable) => {
                writable(path + ".value", binding(value));
                const entry = { dropdown_visible: true, label: reader.text(label), value: reader.read(value),
                    items: list(items.map((item) => descriptionOf(reader,
                        { label: reader.text(item.label), value: Number(item.value) }, item.description))),
                    visible: reader.flag(o.visible, true), disabled: reader.flag(o.disabled, false) };
                descriptionOf(reader, entry, o.description);
                return tooltipOf(reader, entry, o.tooltip);
            });
        }
        header(text, options) {
            this.#screen.guard();
            const o = optionsOf(options);
            return this.#add((reader) =>
                tooltipOf(reader, { header_visible: true, text: reader.text(text), visible: reader.flag(o.visible, true) }, o.tooltip));
        }
        image(src, pack, options) {
            this.#screen.guard();
            const o = optionsOf(options);
            if (o.onClick !== undefined && typeof o.onClick !== "function") throw conversion();
            return this.#add((reader, path, writable) => {
                const entry = { image_visible: true, image_src: reader.string(src), image_pack: reader.string(pack),
                    image_clickable: o.onClick !== undefined, image_onClick: 0, visible: reader.flag(o.visible, true) };
                if (o.onClick !== undefined) writable(path + ".image_onClick", action(o.onClick));
                if (o.width !== undefined) entry.image_width = reader.number(o.width);
                return o.tooltip === undefined ? entry : optional(entry, "image_tooltip", reader.text(o.tooltip));
            });
        }
        isShowing() {
            return this.#screen.showing();
        }
        label(text, options) {
            this.#screen.guard();
            const o = optionsOf(options);
            return this.#add((reader) =>
                tooltipOf(reader, { label_visible: true, text: reader.text(text), visible: reader.flag(o.visible, true) }, o.tooltip));
        }
        multiButtonRow(buttons, options) {
            this.#screen.guard();
            if (!Array.isArray(buttons)) throw conversion();
            for (const button of buttons)
                if (button === null || typeof button !== "object" || typeof button.onClick !== "function") throw conversion();
            const o = optionsOf(options);
            return this.#add((reader, path, writable) => {
                const row = buttons.map((button, index) => {
                    writable(path + ".layout[" + index + "].onClick", action(button.onClick));
                    return layoutButton(reader, button.label, button.options);
                });
                return { row_visible: true, visible: reader.flag(o.visible, true), layout: list(row) };
            });
        }
        show() {
            return this.#screen.show((reason) => reason);
        }
        slider(label, value, min, max, options) {
            this.#screen.guard();
            writableObservable(value, ObservableNumber, "slider value");
            const o = optionsOf(options);
            return this.#add((reader, path, writable) => {
                writable(path + ".value", binding(value));
                const entry = { slider_visible: true, label: reader.text(label), value: reader.read(value),
                    minValue: reader.number(min), maxValue: reader.number(max), visible: reader.flag(o.visible, true),
                    disabled: reader.flag(o.disabled, false) };
                if (o.step !== undefined) entry.step = reader.number(o.step);
                if (o.fixedFormatDigits !== undefined) entry.fixedFormatDigits = reader.number(o.fixedFormatDigits);
                descriptionOf(reader, entry, o.description);
                return tooltipOf(reader, entry, o.tooltip);
            });
        }
        spacer(options) {
            this.#screen.guard();
            const o = optionsOf(options);
            return this.#add((reader) => ({ spacer_visible: true, visible: reader.flag(o.visible, true) }));
        }
        textField(label, text, options) {
            this.#screen.guard();
            writableObservable(text, ObservableString, "text");
            const o = optionsOf(options);
            return this.#add((reader, path, writable) => {
                writable(path + ".text", binding(text));
                const entry = { textfield_visible: true, label: reader.text(label), text: reader.read(text),
                    visible: reader.flag(o.visible, true), disabled: reader.flag(o.disabled, false) };
                descriptionOf(reader, entry, o.description);
                return tooltipOf(reader, entry, o.tooltip);
            });
        }
        toggle(label, toggled, options) {
            this.#screen.guard();
            writableObservable(toggled, ObservableBoolean, "toggled value");
            const o = optionsOf(options);
            return this.#add((reader, path, writable) => {
                writable(path + ".toggled", binding(toggled));
                const entry = { toggle_visible: true, label: reader.text(label), toggled: reader.read(toggled),
                    visible: reader.flag(o.visible, true), disabled: reader.flag(o.disabled, false) };
                descriptionOf(reader, entry, o.description);
                return tooltipOf(reader, entry, o.tooltip);
            });
        }
    }

    class MessageBox {
        #screen;
        #title;
        #body = "";
        #horizontal;
        #buttons = [undefined, undefined, undefined];
        #selection;
        constructor(player, title, options) {
            if (!native.isPlayer(player)) throw invalidPlayer(player, "constructor");
            if (typeof title !== "string" && !isRawMessage(title)) throw conversion();
            this.#title = title;
            this.#horizontal = options !== undefined && options !== null && options.showHorizontalButtons === true;
            this.#screen = new Screen(player, "minecraft:message_box", "message_box_data_", (reader, writable) => {
                const value = { title: reader.text(this.#title), body: reader.text(this.#body),
                    is_vertical: !this.#horizontal, is_horizontal: this.#horizontal };
                this.#buttons.forEach((button, index) => {
                    const key = "button" + (index + 1);
                    writable(key + ".onClick", () => {
                        this.#selection = index;
                    });
                    // sub_14B65F7C0: label, onClick and visible, then the optional tooltip and image
                    const entry = { label: button === undefined ? "" : reader.text(button.label), onClick: 0,
                        visible: button !== undefined };
                    if (button !== undefined) {
                        tooltipOf(reader, entry, button.tooltip);
                        imageOf(reader, entry, button.imageDetails);
                    }
                    value[key] = entry;
                });
                return value;
            });
        }
        body(text, options) {
            this.#screen.guard();
            if (typeof text !== "string" && !isRawMessage(text) && !isObservable(text, ObservableString)
                && !isObservable(text, ObservableUIRawMessage)) throw conversion();
            this.#body = text;
            return this;
        }
        #button(index, label, options) {
            this.#screen.guard();
            const o = optionsOf(options);
            this.#buttons[index] = { label, tooltip: o.tooltip, imageDetails: o.imageDetails };
            return this;
        }
        button1(label, tooltip) {
            return this.#button(0, label, { tooltip });
        }
        button1WithOptions(label, options) {
            return this.#button(0, label, options);
        }
        button2(label, tooltip) {
            return this.#button(1, label, { tooltip });
        }
        button2WithOptions(label, options) {
            return this.#button(1, label, options);
        }
        button3WithOptions(label, options) {
            return this.#button(2, label, options);
        }
        close() {
            this.#screen.close();
        }
        isShowing() {
            return this.#screen.showing();
        }
        show() {
            this.#selection = undefined;
            return this.#screen.show((closeReason) => {
                const result = { closeReason };
                if (this.#selection !== undefined) result.selection = this.#selection;
                return result;
            });
        }
    }

    class UIManager {
        constructor(key) {
            if (key !== token) sealed("UIManager");
        }
        closeAllForms(player) {
            native.closeAllForms(player);
        }
    }
    const uiManager = new UIManager(token);

    return { ActionFormData, MessageFormData, ModalFormData, FormResponse, ActionFormResponse, MessageFormResponse,
        ModalFormResponse, UIManager, uiManager, CustomForm, MessageBox, ObservableBoolean, ObservableNumber,
        ObservableString, ObservableUIRawMessage };
})
)JS";

    // GraphicsOverrideParameterType order; ranges from the definitions, checked by the game's setters
    const char *GRAPHICS = R"JS(
(function (native) {
    const token = Symbol();
    const UNSUPPORTED = "Invalid value passed. Value did not match one of the supported values.";
    const invalid = () => native.errors.make("InvalidArgumentError", UNSUPPORTED);
    const conversion = () => new TypeError("Native type conversion failed.");
    const isNumber = (value) => typeof value === "number" && Number.isFinite(value);
    const inRange = (value, range) => range === undefined || (value >= range[0] && value <= range[1]);
    const color = (value) => {
        if (value === null || typeof value !== "object" || !isNumber(value.red) || !isNumber(value.green)
            || !isNumber(value.blue))
            return undefined;
        return [value.red, value.green, value.blue];
    };
    // a single value goes out as one keyframe at time 0; a number fills all three components
    const keyframes = (value, kind, range) => {
        const single = kind === "rgb" ? color(value) : (isNumber(value) ? [value, value, value] : undefined);
        let frames;
        if (single !== undefined) {
            frames = [[0, single]];
        } else {
            if (value === null || typeof value !== "object" || Array.isArray(value))
                throw conversion();
            frames = [];
            for (const key of Object.keys(value)) {
                const time = Number(key);
                const entry = kind === "rgb" ? color(value[key])
                    : (isNumber(value[key]) ? [value[key], value[key], value[key]] : undefined);
                if (!Number.isFinite(time) || entry === undefined)
                    throw conversion();
                if (time < 0 || time > 1)
                    throw invalid();
                frames.push([time, entry]);
            }
        }
        for (const frame of frames) {
            if (kind === "rgb" ? frame[1].some((c) => c < 0 || c > 1) : !inRange(frame[1][0], range))
                throw invalid();
        }
        return frames;
    };
    const SPECS = {
        BiomeAtmospherics: [
            ["SkyZenithColor", 0, "rgb"], ["SkyHorizonColor", 1, "rgb"], ["HorizonBlendMin", 2, "frames", [0, 1]],
            ["HorizonBlendMax", 3, "frames", [0, 1]], ["HorizonBlendStart", 4, "frames", [0, 1]],
            ["HorizonBlendMieStart", 5, "frames", [0, 1.2]], ["RayleighStrength", 6, "frames", [0, 11]],
            ["SunMieStrength", 7, "frames", [0, 60]], ["MoonMieStrength", 8, "frames", [0, 60]],
            ["SunGlareShape", 9, "frames", [0, 50]]],
        BiomeWater: [
            ["Chlorophyll", 10, "number", [0, 10]], ["CDOM", 11, "number", [0, 15]],
            ["SuspendedSediment", 12, "number", [0, 300]], ["WavesDepth", 13, "number", [0, 10]],
            ["WavesFrequency", 14, "number", [0, 10]], ["WavesFrequencyScaling", 15, "number", [0, 2]],
            ["WavesSpeed", 16, "number", [0.01, 10]], ["WavesSpeedScaling", 17, "number", [0, 2]],
            ["WavesShape", 18, "number", [1, 10]], ["WavesOctaves", 19, "number", [1, 10]],
            ["WavesMix", 20, "number", [0, 1]], ["WavesPull", 21, "number", [-1, 1]],
            ["WavesDirectionIncrement", 22, "number", [0, 360]]],
        BiomeColorGrading: [
            ["MidtonesContrast", 23, "vector", [0, 4]], ["HighlightsContrast", 24, "vector", [0, 4]],
            ["ShadowsContrast", 25, "vector", [0, 4]], ["HighlightsGain", 26, "vector", [0, 10]],
            ["HighlightsGamma", 27, "vector", [0, 4]], ["HighlightsOffset", 28, "vector", [-1, 1]],
            ["HighlightsSaturation", 29, "vector", [0, 10]], ["MidtonesGain", 30, "vector", [0, 10]],
            ["MidtonesGamma", 31, "vector", [0, 4]], ["MidtonesOffset", 32, "vector", [-1, 1]],
            ["MidtonesSaturation", 33, "vector", [0, 10]], ["ShadowsGain", 34, "vector", [0, 10]],
            ["ShadowsGamma", 35, "vector", [0, 4]], ["ShadowsOffset", 36, "vector", [-1, 1]],
            ["ShadowsSaturation", 37, "vector", [0, 10]], ["HighlightsMin", 38, "number", [1, 20]],
            ["ShadowsMax", 39, "number", [0, 1]], ["Temperature", 40, "number", [1000, 15000]]],
        BiomeLighting: [
            ["SunColor", 41, "rgb"], ["SunIlluminance", 42, "frames"], ["MoonColor", 43, "rgb"],
            ["MoonIlluminance", 44, "frames"], ["FlashColor", 45, "rgb"], ["FlashIlluminance", 46, "frames"],
            ["AmbientColor", 47, "rgb"], ["AmbientIlluminance", 48, "frames"],
            ["EmissiveDesaturation", 49, "number"], ["SkyIntensity", 50, "frames"],
            ["OrbitalOffsetDegrees", 51, "frames"]],
    };
    // biome and optional player of each object: the game keeps them on the native side
    const owners = new WeakMap();
    const send = (self, parameter, reset, mode, data) => {
        const owner = owners.get(self);
        native.send(owner.biome, owner.player, parameter, reset, mode, data);
    };
    const classes = {};
    for (const name of Object.keys(SPECS)) {
        const cls = class {
            constructor(key, biome, player) {
                if (key !== token)
                    throw new ReferenceError("No constructor for native class '" + name + "'.");
                owners.set(this, { biome, player });
            }
        };
        Object.defineProperty(cls, "name", { value: name });
        for (const [suffix, parameter, kind, range] of SPECS[name]) {
            Object.defineProperty(cls.prototype, "set" + suffix, { writable: true, configurable: true,
                value: function (value) {
                    if (arguments.length < 1)
                        throw conversion();
                    if (kind === "frames" || kind === "rgb")
                        return send(this, parameter, false, 0, keyframes(value, kind, range));
                    if (kind === "number") {
                        if (!isNumber(value))
                            throw conversion();
                        if (!inRange(value, range))
                            throw invalid();
                        return send(this, parameter, false, 1, [value]);
                    }
                    if (value === null || typeof value !== "object" || !isNumber(value.x) || !isNumber(value.y)
                        || !isNumber(value.z))
                        throw conversion();
                    if (![value.x, value.y, value.z].every((c) => inRange(c, range)))
                        throw invalid();
                    return send(this, parameter, false, 2, [value.x, value.y, value.z]);
                } });
            Object.defineProperty(cls.prototype, "reset" + suffix, { writable: true, configurable: true,
                value: function () {
                    return send(this, parameter, true, 0, []);
                } });
        }
        classes[name] = cls;
    }
    const make = (name, biome, player) => {
        if (!native.isBiome(biome))
            throw conversion();
        if (player !== undefined && !native.isPlayer(player))
            throw conversion();
        return new classes[name](token, biome, player);
    };
    return {
        BiomeAtmospherics: classes.BiomeAtmospherics,
        BiomeColorGrading: classes.BiomeColorGrading,
        BiomeLighting: classes.BiomeLighting,
        BiomeWater: classes.BiomeWater,
        getBiomeAtmospherics: (biome) => make("BiomeAtmospherics", biome),
        getBiomeColorGrading: (biome) => make("BiomeColorGrading", biome),
        getBiomeLighting: (biome) => make("BiomeLighting", biome),
        getBiomeWater: (biome) => make("BiomeWater", biome),
        getPlayerAtmospherics: (biome, player) => make("BiomeAtmospherics", biome, player),
        getPlayerColorGrading: (biome, player) => make("BiomeColorGrading", biome, player),
        getPlayerLighting: (biome, player) => make("BiomeLighting", biome, player),
        getPlayerWater: (biome, player) => make("BiomeWater", biome, player),
    };
})
)JS";
}

namespace {
    const char *LOOT = R"JS(
(function (native) {
    const token = Symbol();
    const conversion = () => new TypeError("Native type conversion failed.");
    const fields = new WeakMap();
    const define = (name, parent, keys) => {
        const cls = parent ? class extends parent {}
            : class {
                constructor(key, data) {
                    if (key !== token)
                        throw new ReferenceError("No constructor for native class '" + new.target.name + "'.");
                    fields.set(this, data);
                }
            };
        Object.defineProperty(cls, "name", { value: name });
        for (const key of keys)
            Object.defineProperty(cls.prototype, key, { configurable: true, get() { return fields.get(this)[key]; } });
        return cls;
    };
    const number = (value, fallback) => (typeof value === "number" && Number.isFinite(value) ? value : fallback);
    // a number, a [min, max] pair or a {min, max} object, as the table files write ranges
    const range = (value, fallback) => {
        if (typeof value === "number")
            return { min: value, max: value };
        if (Array.isArray(value))
            return { min: number(value[0], 0), max: number(value[1], number(value[0], 0)) };
        if (value !== null && typeof value === "object")
            return { min: number(value.min, 0), max: number(value.max, number(value.min, 0)) };
        return { min: fallback, max: fallback };
    };
    const strings = (value) => (Array.isArray(value) ? value.map(String) : []);
    const unprefixed = (name) => String(name || "").replace(/^minecraft:/, "");
    // "loot_tables/chests/x.json" as scripts name it: "chests/x"
    const tablePath = (name) => String(name || "").replace(/^loot_tables\//, "").replace(/\.json$/, "");

    const classes = {};
    const add = (name, parent, keys) => (classes[name] = define(name, parent, keys));
    const LootItemCondition = add("LootItemCondition", null, []);
    const LootItemFunction = add("LootItemFunction", null, ["conditions"]);
    const LootPoolEntry = add("LootPoolEntry", null, ["quality", "subTable", "weight"]);
    const LootItem = add("LootItem", LootPoolEntry, ["conditions", "functions", "name"]);
    const LootTableReference = add("LootTableReference", LootPoolEntry, ["path"]);
    const LootTableEntry = add("LootTableEntry", LootPoolEntry, ["lootTable"]);
    const EmptyLootItem = add("EmptyLootItem", LootPoolEntry, []);
    const LootPool = add("LootPool", null, ["bonusRolls", "conditions", "entries", "rolls", "tiers"]);
    const LootPoolTiers = add("LootPoolTiers", null, ["bonusChance", "bonusRolls", "initialRange"]);
    const LootTable = add("LootTable", null, ["path", "pools"]);
    const EnchantInfo = add("EnchantInfo", null, ["enchantment", "range"]);

    const enchantInfo = (entry) => new EnchantInfo(token, {
        enchantment: String(entry.id ?? entry.enchantment ?? ""), range: range(entry.level ?? entry.levels, 1) });
    const difficultyChances = (json) => {
        const fallback = number(json.default_chance, 0);
        return ["peaceful", "easy", "normal", "hard"].map((key) => number(json[key], fallback));
    };
    // condition files name, class, fields
    const CONDITIONS = {
        random_chance: ["RandomChanceCondition", (j) => ({ chance: number(j.chance, 0) })],
        random_chance_with_looting: ["RandomChanceWithLootingCondition",
            (j) => ({ chance: number(j.chance, 0), lootingMultiplier: number(j.looting_multiplier, 0) })],
        random_difficulty_chance: ["RandomDifficultyChanceCondition", (j) => ({ chances: difficultyChances(j) })],
        random_regional_difficulty_chance: ["RandomRegionalDifficultyChanceCondition",
            (j) => ({ maxChance: number(j.max_chance, 0) })],
        killed_by_player: ["KilledByPlayerCondition", () => ({})],
        killed_by_player_or_pets: ["KilledByPlayerOrPetsCondition", () => ({})],
        is_baby: ["IsBabyCondition", () => ({})],
        killed_by_entity: ["KilledByEntityCondition", (j) => ({ entityType: String(j.entity_type ?? "") })],
        entity_killed: ["EntityKilledCondition", (j) => ({ entityType: String(j.entity_type ?? "") })],
        damaged_by_entity: ["DamagedByEntityCondition", (j) => ({ entityType: String(j.entity_type ?? "") })],
        passenger_of_entity: ["PassengerOfEntityCondition", (j) => ({ entityType: String(j.entity_type ?? "") })],
        has_mark_variant: ["EntityHasMarkVariantCondition", (j) => ({ value: number(j.value, 0) })],
        has_variant: ["EntityHasVariantCondition", (j) => ({ value: number(j.value, 0) })],
        match_tool: ["MatchToolCondition", (j) => ({
            count: range(j.count, 0), durability: range(j.durability, 0),
            enchantments: Array.isArray(j.enchantments) ? j.enchantments.map(enchantInfo) : [],
            itemName: String(j.item ?? ""), itemTagsAll: strings(j["minecraft:match_tool_filter_all"]),
            itemTagsAny: strings(j["minecraft:match_tool_filter_any"]),
            itemTagsNone: strings(j["minecraft:match_tool_filter_none"]) })],
    };
    const FUNCTIONS = {
        set_count: ["SetItemCountFunction", (j) => ({ count: range(j.count, 1) })],
        set_damage: ["SetItemDamageFunction", (j) => ({ damage: range(j.damage, 1) })],
        set_data: ["SetItemDataFunction", (j) => ({ data: range(j.data, 0) })],
        looting_enchant: ["LootingEnchantFunction", (j) => ({ count: range(j.count, 0) })],
        explosion_decay: ["ExplosionDecayFunction", () => ({})],
        enchant_random_gear: ["EnchantRandomEquipmentFunction", (j) => ({ chance: number(j.chance, 0) })],
        enchant_randomly: ["EnchantRandomlyFunction", (j) => ({ treasure: j.treasure === true })],
        enchant_with_levels: ["EnchantWithLevelsFunction",
            (j) => ({ levels: range(j.levels, 0), treasure: j.treasure === true })],
        set_potion: ["SetPotionFunction", (j) => ({ id: String(j.id ?? "") })],
        exploration_map: ["ExplorationMapFunction", (j) => ({ destination: String(j.destination ?? "") })],
        specific_enchants: ["SpecificEnchantFunction",
            (j) => ({ enchantments: Array.isArray(j.enchants) ? j.enchants.map(enchantInfo) : [] })],
        furnace_smelt: ["SmeltItemFunction", () => ({})],
        set_data_from_color_index: ["SetDataFromColorIndexFunction", () => ({})],
        set_armor_trim: ["SetArmorTrimFunction",
            (j) => ({ material: String(j.material ?? ""), pattern: String(j.pattern ?? "") })],
        set_ominous_bottle_amplifier: ["SetOminousBottleFunction", (j) => ({ amplifier: range(j.amplifier, 0) })],
        random_aux_value: ["RandomAuxValueFunction", (j) => ({ values: range(j.values, 0) })],
        random_block_state: ["RandomBlockStateFunction",
            (j) => ({ blockState: String(j.block_state ?? ""), values: range(j.values, 0) })],
        random_dye: ["RandomDyeFunction", () => ({})],
        set_banner_details: ["SetBannerDetailsFunction", (j) => ({ type: number(j.type, 0) })],
        set_stew_effect: ["SetStewEffectFunction", (j) => ({
            effects: Array.isArray(j.effects) ? j.effects.map((e) => number(e !== null && typeof e === "object" ? e.id : e, 0)) : [] })],
        set_book_contents: ["SetBookContentsFunction",
            (j) => ({ author: String(j.author ?? ""), pages: strings(j.pages), title: String(j.title ?? "") })],
        set_lore: ["SetItemLoreFunction", (j) => ({ lore: strings(j.lore) })],
        set_name: ["SetItemNameFunction", (j) => ({ name: String(j.name ?? "") })],
        set_actor_id: ["SetSpawnEggFunction", (j) => ({ id: String(j.id ?? "") })],
        fill_container: ["FillContainerFunction", (j) => ({ lootTable: String(j.loot_table ?? "") })],
    };
    for (const spec of Object.values(CONDITIONS))
        add(spec[0], LootItemCondition, Object.keys(spec[1]({})));
    for (const spec of Object.values(FUNCTIONS))
        add(spec[0], LootItemFunction, Object.keys(spec[1]({})));

    const condition = (json) => {
        const spec = CONDITIONS[unprefixed(json && json.condition)];
        return spec ? new classes[spec[0]](token, spec[1](json)) : new LootItemCondition(token, {});
    };
    const conditions = (list) => (Array.isArray(list) ? list.map(condition) : []);
    const lootFunction = (json) => {
        const spec = FUNCTIONS[unprefixed(json && json.function)];
        const data = spec ? spec[1](json) : {};
        data.conditions = conditions(json && json.conditions);
        return new (spec ? classes[spec[0]] : LootItemFunction)(token, data);
    };
    const entry = (json) => {
        const base = { quality: number(json.quality, 0), weight: number(json.weight, 1), subTable: undefined };
        if (Array.isArray(json.pools))
            base.subTable = new LootTableEntry(token, Object.assign({}, base, { lootTable: table("", json) }));
        switch (json.type) {
            case "item":
                return new LootItem(token, Object.assign(base, { conditions: conditions(json.conditions),
                    functions: Array.isArray(json.functions) ? json.functions.map(lootFunction) : [],
                    name: native.ItemTypes ? native.ItemTypes.get(String(json.name ?? "")) : undefined }));
            case "loot_table":
                return new LootTableReference(token, Object.assign(base, { path: tablePath(json.name) }));
            default:
                return new EmptyLootItem(token, base);
        }
    };
    const pool = (json) => new LootPool(token, {
        bonusRolls: range(json.bonus_rolls, 0), conditions: conditions(json.conditions),
        entries: Array.isArray(json.entries) ? json.entries.map(entry) : [], rolls: range(json.rolls, 1),
        tiers: json.tiers !== null && typeof json.tiers === "object" ? new LootPoolTiers(token, {
            bonusChance: number(json.tiers.bonus_chance, 0), bonusRolls: number(json.tiers.bonus_rolls, 0),
            initialRange: number(json.tiers.initial_range, 1) }) : undefined });
    const table = (path, json) => new LootTable(token, {
        path, pools: Array.isArray(json.pools) ? json.pools.map(pool) : [] });

    const tool = (value) => {
        if (value !== undefined && !native.isItemStack(value))
            throw conversion();
        return value;
    };
    const LootTableManager = add("LootTableManager", null, []);
    Object.assign(LootTableManager.prototype, {
        getLootTable(path) {
            if (typeof path !== "string")
                throw conversion();
            const json = native.definition(path);
            return json === undefined ? undefined : table(tablePath(path), json);
        },
        generateLootFromTable(lootTable, item) {
            if (!(lootTable instanceof LootTable))
                throw conversion();
            return native.fromTable(lootTable.path, tool(item));
        },
        generateLootFromBlock(block, item) {
            return native.fromBlock(block, tool(item));
        },
        generateLootFromBlockPermutation(permutation, item) {
            return native.fromPermutation(permutation, tool(item));
        },
        generateLootFromBlockType(blockType, item) {
            if (blockType === null || typeof blockType !== "object" || typeof blockType.id !== "string")
                throw conversion();
            return native.fromBlockType(blockType.id, tool(item));
        },
        generateLootFromEntity(entity, item) {
            return native.fromEntity(entity, tool(item));
        },
        generateLootFromEntityType(entityType, item) {
            return native.fromEntityType(entityType, tool(item));
        },
    });
    for (const key of Object.keys(LootTableManager.prototype))
        Object.defineProperty(LootTableManager.prototype, key, { enumerable: false });
    classes.manager = new LootTableManager(token, {});
    return classes;
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

    const char *graphics() {
        return GRAPHICS;
    }

    const char *volumes() {
        return VOLUMES;
    }

    const char *loot() {
        return LOOT;
    }
}
