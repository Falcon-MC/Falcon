#include "Scripting/Binding/ScriptGameTestBootstrap.h"

namespace {
    // the GameTest framework: registrations, the Test helper of each running instance, its sequences and its
    // errors. The runner in ScriptGameTests places the structures and reports; this side runs the test code.
    // Registration defaults from sub_14BA2A220 (26.60), sequence steps from GameTestSequence
    const char *SOURCE = R"JS(
(function (native) {
    const token = Symbol();
    const S = native.server;
    const isEntity = (value) => value instanceof S.Entity;
    const conversion = () => new TypeError("Native type conversion failed.");
    // cls undefined for a module function
    const guard = (label, cls, member) => {
        const flags = native.flags(cls === undefined ? member : cls + "." + member);
        const where = cls === undefined ? member : cls + "::" + member;
        const privilege = native.privilege();
        if (privilege === 1 && !flags.early)
            throw new ReferenceError(label + " [" + where + "] cannot be used in early execution.");
        if (privilege === 2 && flags.blocked)
            throw new ReferenceError(label + " [" + where + "] cannot be used in restricted execution.");
    };
    const isNumber = (value) => typeof value === "number" && Number.isFinite(value);
    const number = (value) => {
        if (typeof value !== "number")
            throw conversion();
        return value;
    };
    const integer = (value) => Math.trunc(number(value));
    const string = (value) => {
        if (typeof value !== "string")
            throw conversion();
        return value;
    };
    const bool = (value) => {
        if (typeof value !== "boolean")
            throw conversion();
        return value;
    };
    const fn = (value) => {
        if (typeof value !== "function")
            throw conversion();
        return value;
    };
    const vector3 = (value) => {
        if (value === null || typeof value !== "object" || !isNumber(value.x) || !isNumber(value.y)
            || !isNumber(value.z))
            throw conversion();
        return { x: value.x, y: value.y, z: value.z };
    };
    const blockVector = (value) => {
        const v = vector3(value);
        return { x: Math.floor(v.x), y: Math.floor(v.y), z: Math.floor(v.z) };
    };
    const named = (cls, name) => Object.defineProperty(cls, "name", { value: name });
    const privateClass = (name) => {
        const cls = class {
            constructor(key) {
                if (key !== token)
                    throw new ReferenceError("No constructor for native class '" + name + "'.");
            }
        };
        named(cls, name);
        return cls;
    };
    const define = (cls, className, member, run, getter) => {
        const label = getter ? "Native property getter" : "Native function";
        const wrapped = function (...args) {
            guard(label, className, member);
            return run(this, ...args);
        };
        Object.defineProperty(cls.prototype, member, getter
            ? { configurable: true, enumerable: false, get: wrapped }
            : { configurable: true, writable: true, enumerable: false, value: wrapped });
    };
    const withNamespace = (id) => (id.includes(":") ? id : "minecraft:" + id);
    const typeIdOf = (value) => {
        if (typeof value === "string")
            return withNamespace(value);
        if (value !== null && typeof value === "object" && typeof value.id === "string")
            return value.id;
        throw conversion();
    };

    // GameTestError: message is the text key, messageParameters fill it in
    const gameTestError = (type, message, parameters, context) => {
        const fields = { type, messageParameters: parameters || [] };
        if (context !== undefined)
            fields.context = context;
        return native.errors.make("GameTestError", message, fields);
    };
    const isGameTestError = (error) => error !== null && typeof error === "object" && error.name === "GameTestError";

    // registrations of this pack by lowercase name: the runner looks names up without case
    const registrations = new Map();
    const tags = new Set(["suite:all", "suite:debug", "suite:default", "suite:disabled", "suite:nextupdate"]);

    const RegistrationBuilder = privateClass("RegistrationBuilder");
    const builders = new WeakMap();
    const builderOf = (self) => {
        const reg = builders.get(self);
        if (reg === undefined)
            throw new ReferenceError("Native function [RegistrationBuilder] object bound to prototype does not exist.");
        return reg;
    };
    const setter = (member, apply) => define(RegistrationBuilder, "RegistrationBuilder", member, (self, value, extra) => {
        apply(builderOf(self), value, extra);
        return self;
    });
    setter("batch", (reg, value) => { reg.batch = string(value); });
    setter("maxAttempts", (reg, value) => { reg.maxAttempts = integer(value); });
    setter("maxTicks", (reg, value) => { reg.maxTicks = integer(value); });
    setter("padding", (reg, value) => { reg.padding = integer(value); });
    setter("required", (reg, value) => { reg.required = bool(value); });
    setter("requiredSuccessfulAttempts", (reg, value) => { reg.requiredSuccessfulAttempts = integer(value); });
    setter("rotateTest", (reg, value) => { reg.rotateTest = bool(value); });
    setter("setupTicks", (reg, value) => { reg.setupTicks = integer(value); });
    setter("structureLocation", (reg, value, dimension) => {
        reg.structureLocation = vector3(value);
        if (dimension === undefined) {
            reg.structureDimension = undefined;
        } else {
            const id = string(dimension);
            if (!native.isDimensionId(id))
                throw native.errors.make("InvalidArgumentError", "Invalid dimension type '" + id + "'.");
            reg.structureDimension = id;
        }
    });
    setter("structureName", (reg, value) => { reg.structureName = string(value); });
    setter("tag", (reg, value) => {
        const tag = string(value);
        if (!reg.tags.includes(tag))
            reg.tags.push(tag);
        tags.add(tag);
    });

    const registerTest = (name, async) => (className, testName, testFunction) => {
        guard("Native function", undefined, name);
        string(className);
        string(testName);
        fn(testFunction);
        const fullName = className + ":" + testName;
        const reg = { name: fullName, className, testName, fn: testFunction, async, batch: "", structureName: fullName,
            maxTicks: 100, setupTicks: 0, padding: 1, required: true, maxAttempts: 1, requiredSuccessfulAttempts: 1,
            rotateTest: false, tags: [], structureLocation: undefined, structureDimension: undefined };
        registrations.set(fullName.toLowerCase(), reg);
        tags.add(className);
        const builder = new RegistrationBuilder(token);
        builders.set(builder, reg);
        return builder;
    };

    const batchCallbacks = { before: new Map(), after: new Map() };
    const setBatchCallback = (kind, member) => (batchName, batchCallback) => {
        guard("Native function", undefined, member);
        string(batchName);
        fn(batchCallback);
        if (kind === "before" && batchCallbacks.before.has(batchName))
            throw new Error("GameTest batch " + batchName + " has already been assigned a BeforeBatchCallback.");
        batchCallbacks[kind].set(batchName, batchCallback);
    };

    // rotation steps turn the structure inside its own box: relative (0, 1, 0) is the block over the structure block
    const rotateCell = (v, rotation, size, block) => {
        const sx = block ? size.x - 1 : size.x;
        const sz = block ? size.z - 1 : size.z;
        switch (rotation) {
            case 1: return { x: sz - v.z, y: v.y, z: v.x };
            case 2: return { x: sx - v.x, y: v.y, z: sz - v.z };
            case 3: return { x: v.z, y: v.y, z: sx - v.x };
            default: return { x: v.x, y: v.y, z: v.z };
        }
    };
    const unrotateCell = (v, rotation, size, block) => {
        const sx = block ? size.x - 1 : size.x;
        const sz = block ? size.z - 1 : size.z;
        switch (rotation) {
            case 1: return { x: v.z, y: v.y, z: sz - v.x };
            case 2: return { x: sx - v.x, y: v.y, z: sz - v.z };
            case 3: return { x: sx - v.z, y: v.y, z: v.x };
            default: return { x: v.x, y: v.y, z: v.z };
        }
    };
    const DIRECTIONS = ["South", "West", "North", "East"];

    const GameTestSequence = privateClass("GameTestSequence");
    const sequences = new WeakMap();
    const sequenceOf = (self) => sequences.get(self);
    // a step returns true when done; throwing a Waiting error keeps it for the next tick
    const waiting = () => gameTestError("Waiting", "Waiting");
    const addStep = (self, step) => {
        const seq = sequenceOf(self);
        seq.events.push(step);
        return self;
    };
    const runOrFail = (inst, callback) => {
        try {
            callback();
        } catch (error) {
            fail(inst, error);
        }
    };
    define(GameTestSequence, "GameTestSequence", "thenExecute", (self, callback) => {
        fn(callback);
        const seq = sequenceOf(self);
        return addStep(self, { run: () => runOrFail(seq.inst, callback) });
    });
    define(GameTestSequence, "GameTestSequence", "thenExecuteAfter", (self, delay, callback) => {
        const ticks = integer(delay);
        fn(callback);
        const seq = sequenceOf(self);
        return addStep(self, { run: () => {
            if (seq.inst.tickCount < seq.lastTick + ticks)
                throw waiting();
            runOrFail(seq.inst, callback);
        } });
    });
    define(GameTestSequence, "GameTestSequence", "thenExecuteFor", (self, tickCount, callback) => {
        const ticks = integer(tickCount);
        fn(callback);
        const seq = sequenceOf(self);
        return addStep(self, { run: () => {
            if (seq.inst.tickCount < seq.lastTick + ticks) {
                runOrFail(seq.inst, callback);
                throw waiting();
            }
        } });
    });
    define(GameTestSequence, "GameTestSequence", "thenFail", (self, errorMessage) => {
        const message = string(errorMessage);
        const seq = sequenceOf(self);
        addStep(self, { run: () => fail(seq.inst, gameTestError("Assert", message)) });
    });
    define(GameTestSequence, "GameTestSequence", "thenIdle", (self, delay) => {
        const ticks = integer(delay);
        const seq = sequenceOf(self);
        return addStep(self, { run: () => {
            if (seq.inst.tickCount < seq.lastTick + ticks)
                throw waiting();
        } });
    });
    define(GameTestSequence, "GameTestSequence", "thenSucceed", (self) => {
        const seq = sequenceOf(self);
        addStep(self, { run: () => succeed(seq.inst) });
    });
    define(GameTestSequence, "GameTestSequence", "thenWait", (self, callback) => {
        fn(callback);
        return addStep(self, { run: () => callback() });
    });
    define(GameTestSequence, "GameTestSequence", "thenWaitAfter", (self, delay, callback) => {
        const ticks = integer(delay);
        fn(callback);
        return addStep(self, { run: () => callback(), expectedDelay: ticks });
    });
    // tickAndContinue: a failing step waits; past the time limit the error stands
    const tickSequence = (seq, failIfIncomplete) => {
        while (seq.events.length > 0 && !seq.inst.done) {
            const step = seq.events[0];
            try {
                step.run();
            } catch (error) {
                if (failIfIncomplete)
                    fail(seq.inst, error);
                return;
            }
            seq.events.shift();
            const elapsed = seq.inst.tickCount - seq.lastTick;
            const previous = seq.lastTick;
            seq.lastTick = seq.inst.tickCount;
            if (step.expectedDelay !== undefined && step.expectedDelay !== elapsed) {
                fail(seq.inst, gameTestError("Assert", "gameTest.error.succeededInInvalidTick",
                    [String(previous + step.expectedDelay), String(seq.inst.tickCount)]));
                return;
            }
        }
    };

    const Test = privateClass("Test");
    const instances = new Map();
    const testOf = new WeakMap();

    const finish = (inst, error) => {
        if (inst.done)
            return;
        inst.done = true;
        inst.error = error;
        inst.cleaning = true;
        for (const callback of inst.onFinish.splice(0)) {
            try {
                callback();
            } catch (e) {
                native.log(String(e && e.stack ? e.stack : e));
            }
        }
        inst.cleaning = false;
        // the test's simulated players leave with it
        for (const player of inst.players.splice(0))
            native.sp.remove(player);
        native.finished(inst.id, error === undefined ? undefined : describeError(inst, error));
    };
    const succeed = (inst) => finish(inst, undefined);
    const fail = (inst, error) => {
        if (inst.done)
            return;
        finish(inst, error === undefined ? gameTestError("Unknown", "") : error);
    };
    const describeError = (inst, error) => {
        if (isGameTestError(error))
            return { message: String(error.message), parameters: error.messageParameters.map(String),
                type: error.type, context: error.context };
        if (error instanceof Error)
            return { message: String(error.message), parameters: [], type: "Unknown",
                stack: error.stack === undefined ? undefined : String(error.stack) };
        return { message: String(error), parameters: [], type: "Unknown" };
    };

    const worldBlock = (inst, rel) => {
        const r = rotateCell(blockVector(rel), inst.rotation, inst.size, true);
        return { x: inst.origin.x + r.x, y: inst.origin.y + r.y, z: inst.origin.z + r.z };
    };
    const worldVec = (inst, rel) => {
        const r = rotateCell(vector3(rel), inst.rotation, inst.size, false);
        return { x: inst.origin.x + r.x, y: inst.origin.y + r.y, z: inst.origin.z + r.z };
    };
    const relativeBlock = (inst, world) => {
        const w = blockVector(world);
        return unrotateCell({ x: w.x - inst.origin.x, y: w.y - inst.origin.y, z: w.z - inst.origin.z }, inst.rotation,
            inst.size, true);
    };
    const relativeVec = (inst, world) => {
        const w = vector3(world);
        return unrotateCell({ x: w.x - inst.origin.x, y: w.y - inst.origin.y, z: w.z - inst.origin.z }, inst.rotation,
            inst.size, false);
    };
    // the test area: the structure over its structure block, turned
    const area = (inst) => {
        const turned = inst.rotation === 1 || inst.rotation === 3;
        const sx = turned ? inst.size.z : inst.size.x;
        const sz = turned ? inst.size.x : inst.size.z;
        return { min: { x: inst.origin.x, y: inst.origin.y + 1, z: inst.origin.z },
            max: { x: inst.origin.x + sx, y: inst.origin.y + 1 + inst.size.y, z: inst.origin.z + sz } };
    };
    const atPosition = (inst, key, parameters, rel, type) => {
        const block = blockVector(rel);
        return gameTestError(type || "AssertAtPosition", key, parameters, { absolutePosition: worldBlock(inst, block),
            relativePosition: block, tickCount: inst.tickCount });
    };
    const blockAt = (inst, rel) => {
        const at = worldBlock(inst, rel);
        return inst.dimension.getBlock(at);
    };
    const entitiesInBox = (inst, min, max, typeId) => native.entitiesIn(inst.dimension, min.x, min.y, min.z, max.x,
        max.y, max.z, typeId === undefined ? "" : typeId);
    // a block cell grown by the search distance
    const entitiesAt = (inst, rel, distance, typeId) => {
        const at = worldBlock(inst, rel);
        const d = distance || 0;
        return entitiesInBox(inst, { x: at.x - d, y: at.y - d, z: at.z - d },
            { x: at.x + 1 + d, y: at.y + 1 + d, z: at.z + 1 + d }, typeId);
    };

    const testMethod = (member, run, options) => define(Test, "Test", member, (self, ...args) => {
        const inst = testOf.get(self);
        if (inst === undefined)
            throw new ReferenceError("Native function [Test::" + member + "] object bound to prototype does not exist.");
        if (inst.done && !(options && options.always)) {
            if (inst.cleaning && options && options.cleanup)
                return run(inst, ...args);
            throw native.errors.make("GameTestCompletedError", inst.cleaning
                ? "GameTest " + inst.reg.name + " is currently cleaning up after running and can no longer access method: "
                    + member
                : "GameTest " + inst.reg.name + " has already completed and can no longer access method: " + member,
                { reason: inst.cleaning ? "Cleanup" : "Done" });
        }
        return run(inst, ...args);
    });
    const world = { cleanup: true };

    testMethod("assert", (inst, condition, message) => {
        bool(condition);
        string(message);
        if (!condition)
            throw gameTestError("Assert", message);
    });
    testMethod("assertBlockPresent", (inst, blockType, location, isPresent) => {
        const typeId = typeIdOf(blockType);
        const present = isPresent === undefined ? true : bool(isPresent);
        const block = blockAt(inst, location);
        const matches = block !== undefined && block.typeId === typeId;
        if (present && !matches)
            throw atPosition(inst, "gameTest.assert.expectBlock", [typeId], location);
        if (!present && matches)
            throw atPosition(inst, "gameTest.assert.unexpectedBlock", [typeId], location);
    }, world);
    testMethod("assertBlockState", (inst, location, callback) => {
        fn(callback);
        if (!callback(blockAt(inst, location)))
            throw atPosition(inst, "gameTest.assert.unexpectedBlockState", [], location);
    }, world);
    testMethod("assertCanReachLocation", (inst, mob, location, canReach) => {
        const reach = canReach === undefined ? true : bool(canReach);
        const at = worldBlock(inst, location);
        const reached = native.canReach(mob, at.x, at.y, at.z);
        if (reached === undefined)
            throw conversion();
        if (reach && !reached)
            throw atPosition(inst, "gameTest.assert.noPathFound", [mob.typeId], location);
        if (!reach && reached)
            throw atPosition(inst, "gameTest.assert.pathFound", [mob.typeId], location);
    }, world);
    // the block's inventory component, every stack it holds
    const containerAt = (inst, location) => {
        const block = blockAt(inst, location);
        const inventory = block === undefined ? undefined : block.getComponent("minecraft:inventory");
        if (inventory === undefined || inventory.container === undefined)
            throw atPosition(inst, "gameTest.assert.noContainerFound", [], location);
        const items = [];
        const container = inventory.container;
        for (let slot = 0; slot < container.size; slot++) {
            const item = container.getItem(slot);
            if (item !== undefined)
                items.push(item);
        }
        return items;
    };
    testMethod("assertContainerContains", (inst, itemStack, location) => {
        if (!(itemStack instanceof S.ItemStack))
            throw conversion();
        const items = containerAt(inst, location);
        if (!items.some((item) => item.typeId === itemStack.typeId))
            throw atPosition(inst, "gameTest.assert.expectItemInContainer", [itemStack.typeId], location);
    }, world);
    testMethod("assertContainerEmpty", (inst, location) => {
        if (containerAt(inst, location).length > 0)
            throw atPosition(inst, "gameTest.assert.expectEmptyContainer", [], location);
    }, world);
    testMethod("assertEntityHasArmor", (inst, typeId, armorSlot, armorName, armorData, location, hasArmor) => {
        const type = withNamespace(string(typeId));
        const slot = integer(armorSlot);
        const name = withNamespace(string(armorName));
        const data = integer(armorData);
        const expected = hasArmor === undefined ? true : bool(hasArmor);
        const found = entitiesAt(inst, location, 0, type).some((entity) => native.hasArmor(entity, slot, name, data));
        if (expected && !found)
            throw atPosition(inst, "gameTest.assert.missingArmor", [name, String(slot), String(data)], location);
        if (!expected && found)
            throw atPosition(inst, "gameTest.assert.hasArmor", [name, String(slot), String(data)], location);
    }, world);
    testMethod("assertEntityHasComponent", (inst, typeId, componentId, location, hasComponent) => {
        const type = withNamespace(string(typeId));
        const component = withNamespace(string(componentId));
        const expected = hasComponent === undefined ? true : bool(hasComponent);
        const entities = entitiesAt(inst, location, 0, type);
        if (entities.length === 0)
            throw atPosition(inst, "gameTest.assert.expectEntityOfType", [type], location);
        const found = entities.some((entity) => entity.hasComponent(component));
        if (expected && !found)
            throw atPosition(inst, "gameTest.assert.missingComponent", [component], location);
        if (!expected && found)
            throw atPosition(inst, "gameTest.assert.hasComponent", [component], location);
    }, world);
    const instanceCheck = (inst, entity, entities, present, location) => {
        if (!isEntity(entity))
            throw conversion();
        if (!entity.isValid)
            throw gameTestError("Assert", "gameTest.assert.invalidEntityInstance");
        const found = entities.some((other) => other === entity);
        if (present && !found)
            throw location === undefined ? gameTestError("Assert", "gameTest.assert.expectEntityInstance", [entity.typeId])
                : atPosition(inst, "gameTest.assert.expectEntityInstance", [entity.typeId], location);
        if (!present && found)
            throw location === undefined
                ? gameTestError("Assert", "gameTest.assert.didNotExpectEntityInstance", [entity.typeId])
                : atPosition(inst, "gameTest.assert.didNotExpectEntityInstance", [entity.typeId], location);
    };
    testMethod("assertEntityInstancePresent", (inst, entity, location, isPresent) => {
        const present = isPresent === undefined ? true : bool(isPresent);
        blockVector(location);
        instanceCheck(inst, entity, isEntity(entity) && entity.isValid ? entitiesAt(inst, location, 0) : [],
            present, location);
    }, world);
    testMethod("assertEntityInstancePresentInArea", (inst, entity, isPresent) => {
        const present = isPresent === undefined ? true : bool(isPresent);
        const box = area(inst);
        instanceCheck(inst, entity, isEntity(entity) && entity.isValid ? entitiesInBox(inst, box.min, box.max) : [],
            present);
    }, world);
    testMethod("assertEntityPresent", (inst, typeId, location, searchDistance, isPresent) => {
        const type = withNamespace(string(typeId));
        const distance = searchDistance === undefined ? 0 : number(searchDistance);
        const present = isPresent === undefined ? true : bool(isPresent);
        const found = entitiesAt(inst, location, distance, type).length > 0;
        if (present && !found)
            throw atPosition(inst, "gameTest.assert.expectEntityOfType", [type], location);
        if (!present && found)
            throw atPosition(inst, "gameTest.assert.didNotExpectEntityOfType", [type], location);
    }, world);
    testMethod("assertEntityPresentInArea", (inst, typeId, isPresent) => {
        const type = withNamespace(string(typeId));
        const present = isPresent === undefined ? true : bool(isPresent);
        const box = area(inst);
        const found = entitiesInBox(inst, box.min, box.max, type).length > 0;
        if (present && !found)
            throw gameTestError("Assert", "gameTest.assert.expectEntityOfType", [type]);
        if (!present && found)
            throw gameTestError("Assert", "gameTest.assert.didNotExpectEntityOfType", [type]);
    }, world);
    testMethod("assertEntityState", (inst, location, typeId, callback) => {
        const type = withNamespace(string(typeId));
        fn(callback);
        const entities = entitiesAt(inst, location, 0, type);
        if (entities.length === 0)
            throw atPosition(inst, "gameTest.assert.expectEntityOfType", [type], location);
        if (!callback(entities[0]))
            throw atPosition(inst, "gameTest.assert.unexpectedDataResult", [], location);
    }, world);
    testMethod("assertEntityTouching", (inst, typeId, location, isTouching) => {
        const type = withNamespace(string(typeId));
        const touching = isTouching === undefined ? true : bool(isTouching);
        const at = worldVec(inst, location);
        const found = native.entitiesTouching(inst.dimension, at.x, at.y, at.z, type).length > 0;
        if (touching && !found)
            throw gameTestError("AssertAtPosition", "gameTest.assert.expectEntityOfType", [type],
                { absolutePosition: at, relativePosition: vector3(location), tickCount: inst.tickCount });
        if (!touching && found)
            throw gameTestError("AssertAtPosition", "gameTest.assert.didNotExpectEntityOfType", [type],
                { absolutePosition: at, relativePosition: vector3(location), tickCount: inst.tickCount });
    }, world);
    testMethod("assertIsWaterlogged", (inst, location, isWaterlogged) => {
        const expected = isWaterlogged === undefined ? true : bool(isWaterlogged);
        const block = blockAt(inst, location);
        const waterlogged = block !== undefined && block.isWaterlogged;
        if (expected && !waterlogged)
            throw atPosition(inst, "gameTest.assert.expectWaterLogged", [], location);
        if (!expected && waterlogged)
            throw atPosition(inst, "gameTest.assert.unexpectedWaterLogged", [], location);
    }, world);
    const itemsAt = (inst, itemType, location, distance) => {
        const typeId = typeIdOf(itemType);
        return { typeId, items: entitiesAt(inst, location, distance, "minecraft:item")
            .map((entity) => native.itemOf(entity)).filter((item) => item !== undefined && item.typeId === typeId) };
    };
    testMethod("assertItemEntityCountIs", (inst, itemType, location, searchDistance, count) => {
        const distance = number(searchDistance);
        const expected = integer(count);
        const found = itemsAt(inst, itemType, location, distance);
        const actual = found.items.reduce((sum, item) => sum + item.amount, 0);
        if (actual !== expected)
            throw atPosition(inst, "gameTest.assert.wrongEntityItemCount", [found.typeId, String(expected),
                String(actual)], location);
    }, world);
    testMethod("assertItemEntityPresent", (inst, itemType, location, searchDistance, isPresent) => {
        const distance = searchDistance === undefined ? 0 : number(searchDistance);
        const present = isPresent === undefined ? true : bool(isPresent);
        const found = itemsAt(inst, itemType, location, distance);
        if (present && found.items.length === 0)
            throw atPosition(inst, "gameTest.assert.expectItemOfType", [found.typeId], location);
        if (!present && found.items.length > 0)
            throw atPosition(inst, "gameTest.assert.unexpectedItemOfType", [found.typeId], location);
    }, world);
    testMethod("assertRedstonePower", (inst, location, power) => {
        const expected = integer(power);
        const block = blockAt(inst, location);
        const actual = block === undefined ? -1 : block.getRedstonePower();
        const value = actual === undefined ? -1 : actual;
        if (value !== expected)
            throw atPosition(inst, "gameTest.assert.unexpectedRedstonePowerLevel", [String(expected), String(value)],
                location);
    }, world);
    testMethod("destroyBlock", (inst, location, dropResources) => {
        const drops = dropResources === undefined ? false : bool(dropResources);
        const at = worldBlock(inst, location);
        native.destroy(inst.dimension, at.x, at.y, at.z, drops);
    }, world);
    testMethod("fail", (inst, errorMessage) => {
        throw gameTestError("Assert", string(errorMessage));
    });
    testMethod("failIf", (inst, callback) => {
        fn(callback);
        const seq = newSequence(inst);
        seq.events.push({ run: () => callback() });
        seq.events.push({ run: () => fail(inst, gameTestError("FailConditionsMet", "gameTest.error.failConditionMet")) });
    });
    testMethod("getBlock", (inst, location) => blockAt(inst, location), world);
    testMethod("getDimension", (inst) => inst.dimension, world);
    testMethod("getTestDirection", (inst) => DIRECTIONS[inst.rotation], world);
    testMethod("idle", (inst, tickDelay) => {
        const ticks = integer(tickDelay);
        return new Promise((resolve) => inst.runAt.push({ tick: inst.tickCount + ticks, run: resolve }));
    });
    testMethod("isCleaningUp", (inst) => inst.cleaning, { always: true });
    testMethod("isCompleted", (inst) => inst.done, { always: true });
    testMethod("killAllEntities", (inst) => {
        const box = area(inst);
        for (const entity of entitiesInBox(inst, box.min, box.max))
            if (entity.typeId !== "minecraft:player")
                native.kill(entity);
    }, world);
    testMethod("pressButton", (inst, location) => {
        const at = worldBlock(inst, location);
        if (!native.pressButton(inst.dimension, at.x, at.y, at.z))
            throw atPosition(inst, "gameTest.assert.missingButton", [], location);
    }, world);
    testMethod("print", (inst, text) => native.say(string(text)), world);
    testMethod("pullLever", (inst, location) => {
        const at = worldBlock(inst, location);
        if (!native.pullLever(inst.dimension, at.x, at.y, at.z))
            throw atPosition(inst, "gameTest.assert.missingLever", [], location);
    }, world);
    testMethod("pulseRedstone", (inst, location, duration) => {
        const ticks = integer(duration);
        const at = worldBlock(inst, location);
        inst.dimension.getBlock(at).setType("minecraft:redstone_block");
        inst.runAt.push({ tick: inst.tickCount + ticks, run: () => inst.dimension.getBlock(at).setType("minecraft:air") });
    }, world);
    testMethod("relativeBlockLocation", (inst, location) => relativeBlock(inst, location), world);
    testMethod("relativeLocation", (inst, location) => relativeVec(inst, location), world);
    testMethod("rotateDirection", (inst, direction) => {
        const index = DIRECTIONS.indexOf(string(direction));
        if (index < 0)
            return direction;
        return DIRECTIONS[(index + inst.rotation) % 4];
    }, world);
    testMethod("rotateVector", (inst, vector) => {
        const v = vector3(vector);
        switch (inst.rotation) {
            case 1: return { x: -v.z, y: v.y, z: v.x };
            case 2: return { x: -v.x, y: v.y, z: -v.z };
            case 3: return { x: v.z, y: v.y, z: -v.x };
            default: return v;
        }
    }, world);
    testMethod("runAfterDelay", (inst, delayTicks, callback) => {
        const ticks = integer(delayTicks);
        fn(callback);
        inst.runAt.push({ tick: inst.tickCount + ticks, run: callback });
    });
    testMethod("runAtTickTime", (inst, tick, callback) => {
        const at = integer(tick);
        fn(callback);
        inst.runAt.push({ tick: at, run: callback });
    });
    testMethod("runOnFinish", (inst, callback) => {
        inst.onFinish.push(fn(callback));
    });
    testMethod("setBlockPermutation", (inst, permutation, location) => {
        if (!(permutation instanceof S.BlockPermutation))
            throw conversion();
        blockAt(inst, location).setPermutation(permutation);
    }, world);
    testMethod("setBlockType", (inst, blockType, location) => {
        const typeId = typeIdOf(blockType);
        const block = blockAt(inst, location);
        try {
            block.setType(typeId);
        } catch (e) {
            throw atPosition(inst, "gameTest.assert.couldNotSetBlock", [typeId], location, "LevelStateModificationFailed");
        }
    }, world);
    testMethod("setFluidContainer", (inst, location, type) => {
        const fluid = ["Water", "Lava", "PowderSnow", "Potion"].indexOf(string(type));
        if (fluid < 0)
            throw conversion();
        const at = worldBlock(inst, location);
        if (!native.setCauldron(inst.dimension, at.x, at.y, at.z, fluid))
            throw atPosition(inst, "gameTest.assert.expectCauldron", [], location);
    }, world);
    testMethod("setTntFuse", (inst, entity, fuseLength) => {
        if (!isEntity(entity))
            throw conversion();
        const ticks = integer(fuseLength);
        // sub_144AF6240: the explode component first, then the length, both naming the entity type at the
        // test's relative origin (sub_144B199E0)
        const result = native.setFuse(entity, ticks);
        const origin = { x: 0, y: 0, z: 0 };
        if (result === 1)
            throw atPosition(inst, "gameTest.assert.missingExplosionComponent", [entity.typeId], origin, "Assert");
        if (result === 2)
            throw atPosition(inst, "gameTest.assert.invalidFuseLength", [entity.typeId], origin, "Assert");
    }, world);
    const spawnAt = (inst, typeId, at, behaviors, relative) => {
        const type = withNamespace(string(typeId));
        const entity = behaviors ? native.spawn(inst.dimension, type, at.x, at.y, at.z, true)
            : native.spawn(inst.dimension, type, at.x, at.y, at.z, false);
        if (entity === null)
            throw gameTestError("AssertAtPosition", "gameTest.assert.onlyWorksWithMobs", [],
                { absolutePosition: at, relativePosition: relative, tickCount: inst.tickCount });
        if (entity === undefined)
            throw gameTestError("LevelStateModificationFailed", "gameTest.assert.couldNotSpawnEntity", [type],
                { absolutePosition: at, relativePosition: relative, tickCount: inst.tickCount });
        return entity;
    };
    // a block location spawns at the bottom centre of the block
    const bottomCentre = (inst, location) => {
        const at = worldBlock(inst, location);
        return { x: at.x + 0.5, y: at.y, z: at.z + 0.5 };
    };
    testMethod("spawn", (inst, typeId, location) => spawnAt(inst, typeId, bottomCentre(inst, location), true,
        blockVector(location)), world);
    testMethod("spawnAtLocation", (inst, typeId, location) => spawnAt(inst, typeId, worldVec(inst, location), true,
        vector3(location)), world);
    testMethod("spawnItem", (inst, itemStack, location) => {
        if (!(itemStack instanceof S.ItemStack))
            throw conversion();
        return inst.dimension.spawnItem(itemStack, worldVec(inst, location));
    }, world);
    testMethod("spawnWithoutBehaviors", (inst, typeId, location) => spawnAt(inst, typeId, bottomCentre(inst, location),
        false, blockVector(location)), world);
    testMethod("spawnWithoutBehaviorsAtLocation", (inst, typeId, location) => spawnAt(inst, typeId,
        worldVec(inst, location), false, vector3(location)), world);
    const newSequence = (inst) => {
        const seq = { inst, events: [], lastTick: inst.tickCount };
        inst.sequences.push(seq);
        return seq;
    };
    testMethod("startSequence", (inst) => {
        const sequence = new GameTestSequence(token);
        sequences.set(sequence, newSequence(inst));
        return sequence;
    });
    testMethod("succeed", (inst) => succeed(inst));
    testMethod("succeedIf", (inst, callback) => {
        fn(callback);
        callback();
        succeed(inst);
    });
    testMethod("succeedOnTick", (inst, tick) => {
        const at = integer(tick);
        inst.runAt.push({ tick: at, run: () => succeed(inst) });
    });
    testMethod("succeedOnTickWhen", (inst, tick, callback) => {
        const at = integer(tick);
        fn(callback);
        inst.runAt.push({ tick: at, run: () => {
            callback();
            succeed(inst);
        } });
    });
    const succeedWhen = (inst, check) => {
        const seq = newSequence(inst);
        seq.events.push({ run: check });
        seq.events.push({ run: () => succeed(inst) });
    };
    testMethod("succeedWhen", (inst, callback) => succeedWhen(inst, fn(callback)));
    testMethod("succeedWhenBlockPresent", (inst, blockType, location, isPresent) => {
        const test = inst.test;
        typeIdOf(blockType);
        blockVector(location);
        succeedWhen(inst, () => test.assertBlockPresent(blockType, location, isPresent));
    });
    testMethod("succeedWhenEntityHasComponent", (inst, typeId, componentId, location, hasComponent) => {
        const test = inst.test;
        string(typeId);
        string(componentId);
        bool(hasComponent);
        succeedWhen(inst, () => test.assertEntityHasComponent(typeId, componentId, location, hasComponent));
    });
    testMethod("succeedWhenEntityPresent", (inst, typeId, location, isPresent) => {
        const test = inst.test;
        string(typeId);
        succeedWhen(inst, () => test.assertEntityPresent(typeId, location, 0, isPresent));
    });
    testMethod("until", (inst, callback) => {
        fn(callback);
        return new Promise((resolve) => {
            const seq = newSequence(inst);
            seq.events.push({ run: () => {
                callback();
                resolve();
            } });
        });
    });
    testMethod("walkTo", (inst, mob, location, speedModifier) => {
        const speed = speedModifier === undefined ? 1 : number(speedModifier);
        const at = worldBlock(inst, location);
        if (!native.walkTo(mob, at.x + 0.5, at.y, at.z + 0.5, speed))
            throw atPosition(inst, "gameTest.assert.mobWontMove", [mob.typeId], location);
    }, world);
    testMethod("walkToLocation", (inst, mob, location, speedModifier) => {
        const speed = speedModifier === undefined ? 1 : number(speedModifier);
        const at = worldVec(inst, location);
        if (!native.walkTo(mob, at.x, at.y, at.z, speed))
            throw gameTestError("AssertAtPosition", "gameTest.assert.mobWontMove", [mob.typeId],
                { absolutePosition: at, relativePosition: vector3(location), tickCount: inst.tickCount });
    }, world);
    testMethod("worldBlockLocation", (inst, location) => worldBlock(inst, location), world);
    testMethod("worldLocation", (inst, location) => worldVec(inst, location), world);

    // SimulatedPlayer: a Player the server moves; its locations are relative to the test that spawned it
    const SimulatedPlayer = privateClass("SimulatedPlayer");
    Object.setPrototypeOf(SimulatedPlayer.prototype, S.Player.prototype);
    Object.setPrototypeOf(SimulatedPlayer, S.Player);
    native.sp.prototype(SimulatedPlayer.prototype);
    const playerTests = new WeakMap();
    // sub_144B06CD0, qword_14F1FC468: the body faces the test's turn
    const TEST_YAWS = [0, 90, 180, -90];
    const LOOK_DURATIONS = ["Instant", "Continuous", "UntilMove"];
    const invalidPlayer = (self, member) => {
        const fields = { type: "minecraft:player" };
        try {
            fields.id = S.Player.prototype.__lookupGetter__("id").call(self);
        } catch (error) {
            fields.id = "";
        }
        return native.errors.make("InvalidEntityError", "Failed to call function '" + member
            + "' due to Entity being invalid (has the Entity been removed?).", fields);
    };
    const simulatedMethod = (member, run, getter) => define(SimulatedPlayer, "SimulatedPlayer", member,
        (self, ...args) => {
            if (!(self instanceof SimulatedPlayer))
                throw new ReferenceError("Native function [SimulatedPlayer::" + member
                    + "] object bound to prototype does not exist.");
            if (!native.sp.is(self))
                throw invalidPlayer(self, member);
            return run(self, ...args);
        }, getter);
    const playerWorld = (self, location) => {
        const inst = playerTests.get(self);
        return inst === undefined ? vector3(location) : worldVec(inst, location);
    };
    const playerBlock = (self, location) => {
        const inst = playerTests.get(self);
        return inst === undefined ? blockVector(location) : worldBlock(inst, location);
    };
    const lookDuration = (duration) => {
        if (duration === undefined)
            return 2;
        const index = LOOK_DURATIONS.indexOf(duration);
        if (index < 0)
            throw conversion();
        return index;
    };
    simulatedMethod("headRotation", (self) => {
        const rotation = native.sp.rotationOf(self);
        return { x: rotation.x, y: rotation.y };
    }, true);
    Object.defineProperty(SimulatedPlayer.prototype, "isSprinting", {
        configurable: true, enumerable: false,
        get: function () {
            guard("Native property getter", "SimulatedPlayer", "isSprinting");
            if (!native.sp.is(this))
                throw invalidPlayer(this, "isSprinting");
            return native.sp.sprint(this);
        },
        set: function (value) {
            guard("Native property setter", "SimulatedPlayer", "isSprinting");
            if (!native.sp.is(this))
                throw invalidPlayer(this, "isSprinting");
            native.sp.sprint(this, bool(value));
        },
    });
    simulatedMethod("disconnect", (self) => { native.sp.remove(self); });
    simulatedMethod("jump", (self) => native.sp.jump(self));
    simulatedMethod("lookAtBlock", (self, blockLocation, duration) => {
        const at = playerBlock(self, blockLocation);
        native.sp.look(self, at.x + 0.5, at.y + 0.5, at.z + 0.5, lookDuration(duration));
    });
    simulatedMethod("lookAtEntity", (self, entity, duration) => {
        if (!isEntity(entity))
            throw conversion();
        const index = lookDuration(duration);
        if (!native.sp.lookEntity(self, entity, index))
            throw native.errors.make("InvalidEntityError", "Failed to call function 'lookAtEntity' due to Entity "
                + "being invalid (has the Entity been removed?).", { id: "", type: "" });
    });
    simulatedMethod("lookAtLocation", (self, location, duration) => {
        const at = playerWorld(self, location);
        native.sp.look(self, at.x, at.y, at.z, lookDuration(duration));
    });
    // sub_14DCF9ED0: west-east and north-south turned with the test
    simulatedMethod("move", (self, westEast, northSouth, speed) => {
        let x = number(westEast);
        let z = number(northSouth);
        if (speed !== undefined)
            number(speed);
        const inst = playerTests.get(self);
        switch (inst === undefined ? 0 : inst.rotation) {
            case 1: [x, z] = [-z, x]; break;
            case 2: [x, z] = [-x, -z]; break;
            case 3: [x, z] = [z, -x]; break;
            default: break;
        }
        native.sp.move(self, x, 0, z, true);
    });
    simulatedMethod("moveRelative", (self, leftRight, backwardForward, speed) => {
        const x = number(leftRight);
        const z = number(backwardForward);
        if (speed !== undefined)
            number(speed);
        native.sp.move(self, x, 0, z, false);
    });
    const moveOptions = (options) => {
        if (options === undefined)
            return { faceTarget: true, speed: 1 };
        if (options === null || typeof options !== "object")
            throw conversion();
        return { faceTarget: options.faceTarget === undefined ? true : bool(options.faceTarget),
            speed: options.speed === undefined ? 1 : number(options.speed) };
    };
    // sub_14DCFA890: the bottom centre of the block
    simulatedMethod("moveToBlock", (self, blockLocation, options) => {
        const block = blockVector(blockLocation);
        const opts = moveOptions(options);
        const at = playerWorld(self, { x: block.x + 0.5, y: block.y, z: block.z + 0.5 });
        native.sp.moveTo(self, at.x, at.y, at.z, opts.faceTarget, opts.speed);
    });
    simulatedMethod("moveToLocation", (self, location, options) => {
        const opts = moveOptions(options);
        const at = playerWorld(self, location);
        native.sp.moveTo(self, at.x, at.y, at.z, opts.faceTarget, opts.speed);
    });
    // sub_14DCF80B0: added to the current yaw
    simulatedMethod("rotateBody", (self, angleInDegrees) => {
        const angle = number(angleInDegrees);
        native.sp.yaw(self, native.sp.rotationOf(self).body + angle);
    });
    // sub_14DCF8120: turned with the test
    simulatedMethod("setBodyRotation", (self, angleInDegrees) => {
        const angle = number(angleInDegrees);
        const rotation = native.sp.rotation(self);
        native.sp.yaw(self, angle + (rotation > 0 && rotation < 4 ? TEST_YAWS[rotation] : 0));
    });
    simulatedMethod("stopMoving", (self) => { native.sp.stop(self); });
    simulatedMethod("swim", (self) => { native.sp.swim(self, true); });
    simulatedMethod("stopSwimming", (self) => { native.sp.swim(self, false); });
    simulatedMethod("glide", (self) => native.sp.glide(self));
    simulatedMethod("stopGliding", (self) => { native.sp.stopGliding(self); });

    const spawnPlayer = (dimension, at, yaw, name, gameMode, rotation) => {
        const mode = native.sp.gameMode(string(gameMode));
        if (mode < 0)
            throw conversion();
        return native.sp.spawn(dimension, at.x, at.y, at.z, yaw, name, mode, rotation);
    };
    testMethod("spawnSimulatedPlayer", (inst, blockLocation, name, gameMode) => {
        const block = blockVector(blockLocation);
        const playerName = name === undefined ? "Simulated Player" : string(name);
        const at = worldBlock(inst, block);
        const player = spawnPlayer(inst.dimension, { x: at.x + 0.5, y: at.y, z: at.z + 0.5 },
            TEST_YAWS[inst.rotation] || 0, playerName, gameMode === undefined ? "Survival" : gameMode, inst.rotation);
        if (player === undefined)
            throw atPosition(inst, "gameTest.assert.couldNotSpawnEntity", ["Simulated Player"], block,
                "LevelStateModificationFailed");
        playerTests.set(player, inst);
        inst.players.push(player);
        return player;
    }, world);
    testMethod("removeSimulatedPlayer", (inst, simulatedPlayer) => {
        if (!(simulatedPlayer instanceof SimulatedPlayer))
            throw conversion();
        native.sp.remove(simulatedPlayer);
        const index = inst.players.indexOf(simulatedPlayer);
        if (index >= 0)
            inst.players.splice(index, 1);
    }, world);
    const spawnSimulatedPlayer = function (location, name, gameMode) {
        guard("Native function", undefined, "spawnSimulatedPlayer");
        if (location === null || typeof location !== "object" || !(location.dimension instanceof S.Dimension))
            throw conversion();
        const at = vector3(location);
        const player = spawnPlayer(location.dimension, at, 0, string(name), gameMode, 0);
        if (player === undefined)
            throw native.errors.make("EngineError", "Failed to spawn simulated player");
        return player;
    };

    const Tags = privateClass("Tags");
    for (const [key, value] of [["suiteAll", "suite:all"], ["suiteDebug", "suite:debug"],
        ["suiteDefault", "suite:default"], ["suiteDisabled", "suite:disabled"], ["suiteNextUpdate", "suite:nextupdate"]])
        Object.defineProperty(Tags, key, { value, enumerable: true });

    // the runner's side: describe the tests, start, tick and stop instances, run batch callbacks
    const hooks = {
        list: () => Array.from(registrations.values()).map((reg) => ({ name: reg.name, structureName: reg.structureName,
            batch: reg.batch, tags: reg.tags.concat([reg.className, "suite:all"]), maxTicks: reg.maxTicks,
            setupTicks: reg.setupTicks, padding: reg.padding, required: reg.required, maxAttempts: reg.maxAttempts,
            requiredSuccessfulAttempts: reg.requiredSuccessfulAttempts, rotateTest: reg.rotateTest,
            structureLocation: reg.structureLocation, structureDimension: reg.structureDimension })),
        tags: () => Array.from(tags),
        start: (id, name, origin, rotation, size, dimension) => {
            const reg = registrations.get(name.toLowerCase());
            if (reg === undefined)
                return false;
            // the first tick after the setup ticks is tick 0, when the test function runs
            const inst = { id, reg, origin, rotation, size, dimension, tickCount: -reg.setupTicks - 1, runAt: [],
                sequences: [], onFinish: [], players: [], done: false, cleaning: false, error: undefined };
            inst.test = new Test(token);
            testOf.set(inst.test, inst);
            instances.set(id, inst);
            return true;
        },
        stop: (id) => {
            const inst = instances.get(id);
            if (inst === undefined)
                return;
            instances.delete(id);
            inst.done = true;
            for (const player of inst.players.splice(0))
                native.sp.remove(player);
        },
        batch: (kind, name) => {
            const callback = batchCallbacks[kind].get(name);
            if (callback !== undefined)
                callback();
        },
        tick: () => {
            for (const inst of Array.from(instances.values())) {
                if (inst.done) {
                    instances.delete(inst.id);
                    continue;
                }
                inst.tickCount++;
                if (inst.tickCount < 0)
                    continue;
                if (inst.tickCount === 0) {
                    try {
                        const result = inst.reg.fn(inst.test);
                        if (inst.reg.async && result !== null && typeof result === "object"
                            && typeof result.then === "function")
                            result.then(undefined, (error) => fail(inst, error));
                    } catch (error) {
                        fail(inst, error);
                    }
                }
                for (const entry of inst.runAt.slice()) {
                    if (inst.done || entry.tick > inst.tickCount)
                        continue;
                    inst.runAt.splice(inst.runAt.indexOf(entry), 1);
                    try {
                        entry.run();
                    } catch (error) {
                        fail(inst, error);
                    }
                }
                if (inst.done)
                    continue;
                if (inst.tickCount >= inst.reg.maxTicks) {
                    if (inst.sequences.length === 0) {
                        fail(inst, gameTestError("ExecutionTimeout", "gameTest.error.timeout", [String(inst.reg.maxTicks)]));
                    } else {
                        for (const seq of inst.sequences)
                            tickSequence(seq, true);
                        if (!inst.done)
                            fail(inst, gameTestError("ExecutionTimeout", "gameTest.error.noFinishedSequences"));
                    }
                } else {
                    for (const seq of inst.sequences.slice())
                        tickSequence(seq, false);
                }
            }
        },
    };

    return {
        GameTestSequence, RegistrationBuilder, SimulatedPlayer, Tags, Test, spawnSimulatedPlayer,
        register: registerTest("register", false),
        registerAsync: registerTest("registerAsync", true),
        setAfterBatchCallback: setBatchCallback("after", "setAfterBatchCallback"),
        setBeforeBatchCallback: setBatchCallback("before", "setBeforeBatchCallback"),
        hooks,
    };
})
)JS";
}

namespace ScriptGameTestBootstrap {
    const char *source() {
        return SOURCE;
    }
}
