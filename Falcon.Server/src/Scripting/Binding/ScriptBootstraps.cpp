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
            field(this, "canceled", canceled);
            if (reason !== undefined) field(this, "cancelationReason", reason);
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
            const o = options(textFieldOptions, "defaultValue");
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
        // DDUITextFiltering.cpp: without a text filter on the player the native throws the entity error
        getFilteredText(player) {
            throw invalidPlayer(player, "getFilteredText");
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
