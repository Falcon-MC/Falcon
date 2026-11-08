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

    class BlockType {
        #id;
        constructor(key, id) {
            guard(key, "BlockType");
            this.#id = id;
        }
        get id() {
            return this.#id;
        }
    }

    class BlockPermutation {
        #type;
        #states;
        constructor(key, id, states) {
            guard(key, "BlockPermutation");
            this.#type = new BlockType(token, id);
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

    // TODO: drive the engine's ticking areas, script-side bookkeeping for now
    class TickingAreaManager {
        #areas = new Map();
        constructor(key) {
            guard(key, "TickingAreaManager");
        }
        createTickingArea(identifier, options) {
            this.#areas.set(identifier, options || {});
            return Promise.resolve();
        }
        removeTickingArea(identifier) {
            this.#areas.delete(typeof identifier === "string" ? identifier : identifier.identifier);
        }
        getAllTickingAreas() {
            return Array.from(this.#areas.keys()).map((identifier) => ({ identifier }));
        }
    }

    return {
        Scoreboard, ScoreboardObjective, TickingAreaManager,
        scoreboard: new Scoreboard(token), tickingAreaManager: new TickingAreaManager(token)
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

    const char *world() {
        return WORLD;
    }
}
