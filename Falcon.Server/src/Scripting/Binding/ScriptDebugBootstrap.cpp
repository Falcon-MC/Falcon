#include "Scripting/Binding/ScriptDebugBootstrap.h"

namespace {
    // shape defaults from the shape constructors (sub_14B8D88B0 and the per-type ones, 26.60); type ids are
    // ScriptDebugShapeType, the PrimitiveShapes payload order
    const char *SOURCE = R"JS(
(function (native) {
    const token = Symbol();
    const conversion = () => new TypeError("Native type conversion failed.");
    const guard = (label, name) => {
        if (native.privilege() === 1)
            throw new ReferenceError(label + " [" + name + "] cannot be used in early execution.");
    };
    const isNumber = (value) => typeof value === "number" && Number.isFinite(value);
    const number = (value) => {
        if (typeof value !== "number")
            throw conversion();
        return value;
    };
    const bool = (value) => {
        if (typeof value !== "boolean")
            throw conversion();
        return value;
    };
    const vector3 = (value) => {
        if (value === null || typeof value !== "object" || !isNumber(value.x) || !isNumber(value.y)
            || !isNumber(value.z))
            throw conversion();
        return { x: value.x, y: value.y, z: value.z };
    };
    const vector2 = (value) => {
        if (value === null || typeof value !== "object" || !isNumber(value.x) || !isNumber(value.y))
            throw conversion();
        return { x: value.x, y: value.y };
    };
    const rgba = (value) => {
        if (value === null || typeof value !== "object" || !isNumber(value.red) || !isNumber(value.green)
            || !isNumber(value.blue) || !isNumber(value.alpha))
            throw conversion();
        return { red: value.red, green: value.green, blue: value.blue, alpha: value.alpha };
    };
    const players = (value) => {
        if (!Array.isArray(value) || !value.every((player) => native.isPlayer(player)))
            throw conversion();
        return value.slice();
    };
    const bounded = (value, minValue, maxValue) => {
        number(value);
        if (value < minValue || (maxValue !== undefined && value > maxValue)) {
            const fields = { minValue, value };
            if (maxValue !== undefined)
                fields.maxValue = maxValue;
            throw native.errors.make("ArgumentOutOfBoundsError", "", fields);
        }
        return value;
    };
    // a DimensionLocation names its dimension; a bare Vector3 shows in every dimension
    const readLocation = (value) => {
        const location = vector3(value);
        if (value.dimension === undefined)
            return { location, dimension: undefined };
        if (native.dimensionId(value.dimension) < 0)
            throw conversion();
        return { location, dimension: value.dimension };
    };

    const TYPES = { Line: 0, Box: 1, Sphere: 2, Circle: 3, Text: 4, Arrow: 5, Cylinder: 6, Pyramid: 7, Ellipsoid: 8,
        Cone: 9 };
    const states = new WeakMap();
    const unbound = (label, name) => new ReferenceError(label + " [" + name + "] object bound to prototype does not exist.");
    const stateOf = (shape, label, name) => {
        const state = states.get(shape);
        if (state === undefined)
            throw unbound(label, name);
        return state;
    };
    const drawn = new Set();
    let frame = 0;
    const removals = [];

    const accessor = (cls, className, name, read, write) => {
        const descriptor = { configurable: true, enumerable: false,
            get: function () {
                guard("Native property getter", className + "::" + name);
                return read(stateOf(this, "Native property getter", className + "::" + name));
            } };
        if (write !== undefined)
            descriptor.set = function (value) {
                guard("Native property setter", className + "::" + name);
                const state = stateOf(this, "Native property setter", className + "::" + name);
                write(state, value);
                state.dirty = true;
            };
        Object.defineProperty(cls.prototype, name, descriptor);
    };
    const method = (cls, className, name, run) => {
        Object.defineProperty(cls.prototype, name, { configurable: true, writable: true, enumerable: false,
            value: function (...args) {
                guard("Native function", className + "::" + name);
                return run(stateOf(this, "Native function", className + "::" + name), this, ...args);
            } });
    };
    const named = (cls, name) => Object.defineProperty(cls, "name", { value: name });

    const removeShape = (state) => {
        if (!drawn.has(state))
            return;
        drawn.delete(state);
        for (const viewer of state.viewers)
            removals.push([viewer, state.id]);
        state.viewers = new Set();
    };

    // DebugShape and the server's PrimitiveShape are one native shape (sub_14B8CF080); only the debug one has
    // defaultVisibleToAll and hiddenFrom
    const baseClass = (name, debug) => {
        const cls = class {
            constructor(key, type, location, extra) {
                if (key !== token)
                    throw new ReferenceError("No constructor for native class '" + name + "'.");
                const read = readLocation(location);
                states.set(this, Object.assign({ type, location: read.location, dimension: read.dimension, scale: 1,
                    rotation: { x: 0, y: 0, z: 0 }, color: { red: 1, green: 1, blue: 1, alpha: 1 },
                    timeLeft: undefined, totalTimeLeft: undefined, maximumRenderDistance: undefined,
                    attachedTo: undefined, visibleTo: [], hiddenFrom: [], defaultVisibleToAll: true, id: 0,
                    viewers: new Set(), dirty: true, shape: this, server: false }, extra));
            }
        };
        named(cls, name);
        accessor(cls, name, "attachedTo", (s) => s.attachedTo, (s, value) => {
            if (value !== undefined && !native.isEntity(value))
                throw conversion();
            s.attachedTo = value;
        });
        accessor(cls, name, "color", (s) => Object.assign({}, s.color),
            (s, value) => { s.color = rgba(value); });
        accessor(cls, name, "dimension", (s) => s.dimension);
        accessor(cls, name, "hasDuration", (s) => s.timeLeft !== undefined);
        accessor(cls, name, "location", (s) => Object.assign({}, s.location));
        accessor(cls, name, "maximumRenderDistance", (s) => s.maximumRenderDistance, (s, value) => {
            s.maximumRenderDistance = value === undefined ? undefined : bounded(value, 0);
        });
        accessor(cls, name, "rotation", (s) => Object.assign({}, s.rotation),
            (s, value) => { s.rotation = vector3(value); });
        accessor(cls, name, "scale", (s) => s.scale, (s, value) => { s.scale = number(value); });
        accessor(cls, name, "timeLeft", (s) => s.timeLeft, (s, value) => {
            s.timeLeft = value === undefined ? undefined : number(value);
            s.totalTimeLeft = s.timeLeft;
        });
        accessor(cls, name, "totalTimeLeft", (s) => s.totalTimeLeft);
        accessor(cls, name, "visibleTo", (s) => s.visibleTo.slice(),
            (s, value) => { s.visibleTo = players(value); });
        method(cls, name, "remove", (s) => { removeShape(s); });
        method(cls, name, "setLocation", (s, self, location) => {
            const read = readLocation(location);
            s.location = read.location;
            s.dimension = read.dimension;
            s.dirty = true;
        });
        if (debug) {
            accessor(cls, name, "defaultVisibleToAll", (s) => s.defaultVisibleToAll,
                (s, value) => { s.defaultVisibleToAll = bool(value); });
            accessor(cls, name, "hiddenFrom", (s) => s.hiddenFrom.slice(),
                (s, value) => { s.hiddenFrom = players(value); });
        }
        return cls;
    };
    const DebugShape = baseClass("DebugShape", true);

    const shapeClass = (name, parent, type, extra, build) => {
        const cls = class extends parent {
            constructor(...args) {
                if (args[0] === token) {
                    super(...args);
                    return;
                }
                guard("Native constructor for", name + "::constructor");
                const fields = Object.assign({}, extra);
                if (build !== undefined)
                    build(fields, ...args);
                super(token, type, args[0], fields);
            }
        };
        named(cls, name);
        return cls;
    };
    const segmentsOf = (s, value) => bounded(value, 3, 128);

    const DebugLine = shapeClass("DebugLine", DebugShape, TYPES.Line, {}, (fields, location, endLocation) => {
        fields.endLocation = vector3(endLocation);
    });
    accessor(DebugLine, "DebugLine", "endLocation", (s) => Object.assign({}, s.endLocation),
        (s, value) => { s.endLocation = vector3(value); });

    const DebugArrow = shapeClass("DebugArrow", DebugLine, TYPES.Arrow, { headLength: 1, headRadius: 0.5,
        headSegments: 4 }, (fields, location, endLocation) => { fields.endLocation = vector3(endLocation); });
    accessor(DebugArrow, "DebugArrow", "headLength", (s) => s.headLength,
        (s, value) => { s.headLength = number(value); });
    accessor(DebugArrow, "DebugArrow", "headRadius", (s) => s.headRadius,
        (s, value) => { s.headRadius = number(value); });
    accessor(DebugArrow, "DebugArrow", "headSegments", (s) => s.headSegments,
        (s, value) => { s.headSegments = segmentsOf(s, value); });

    const DebugBox = shapeClass("DebugBox", DebugShape, TYPES.Box, { bound: { x: 1, y: 1, z: 1 } });
    accessor(DebugBox, "DebugBox", "bound", (s) => Object.assign({}, s.bound),
        (s, value) => { s.bound = vector3(value); });

    // the sphere and the circle keep a fixed 20 segments
    const DebugSphere = shapeClass("DebugSphere", DebugShape, TYPES.Sphere, { segments: 20 });
    const DebugCircle = shapeClass("DebugCircle", DebugShape, TYPES.Circle, { segments: 20 });

    const round = (name, type) => {
        const cls = shapeClass(name, DebugShape, type, { radii: { x: 1, y: 1 }, height: 1, segments: 20 });
        accessor(cls, name, "height", (s) => s.height, (s, value) => { s.height = number(value); });
        accessor(cls, name, "numSegments", (s) => s.segments, (s, value) => { s.segments = segmentsOf(s, value); });
        accessor(cls, name, "radii", (s) => Object.assign({}, s.radii), (s, value) => { s.radii = vector2(value); });
        return cls;
    };
    const DebugCylinder = round("DebugCylinder", TYPES.Cylinder);
    const DebugCone = round("DebugCone", TYPES.Cone);

    const DebugEllipsoid = shapeClass("DebugEllipsoid", DebugShape, TYPES.Ellipsoid,
        { radii3: { x: 1, y: 1, z: 1 }, segments: 20 });
    accessor(DebugEllipsoid, "DebugEllipsoid", "radii", (s) => Object.assign({}, s.radii3),
        (s, value) => { s.radii3 = vector3(value); });
    accessor(DebugEllipsoid, "DebugEllipsoid", "segmentsPerAxis", (s) => s.segments,
        (s, value) => { s.segments = segmentsOf(s, value); });

    const DebugPyramid = shapeClass("DebugPyramid", DebugShape, TYPES.Pyramid,
        { width: 1, depth: undefined, height: 1 });
    accessor(DebugPyramid, "DebugPyramid", "depth", (s) => s.depth,
        (s, value) => { s.depth = value === undefined ? undefined : number(value); });
    accessor(DebugPyramid, "DebugPyramid", "height", (s) => s.height, (s, value) => { s.height = number(value); });
    accessor(DebugPyramid, "DebugPyramid", "width", (s) => s.width, (s, value) => { s.width = number(value); });

    // a RawMessage goes out as its JSON, a string as is
    const textOf = (value) => {
        if (typeof value === "string")
            return value;
        if (value === null || typeof value !== "object")
            throw conversion();
        return native.rawText(value);
    };
    const copyText = (value) => (typeof value === "string" ? value : JSON.parse(JSON.stringify(value)));
    const TEXT_DEFAULTS = { useRotation: false, backgroundColorOverride: undefined, depthTest: false,
        backfaceVisible: true, textBackfaceVisible: false, lineGapHeight: 0 };
    const textFields = (fields, location, text) => {
        fields.wire = textOf(text);
        fields.text = copyText(text);
    };
    const DebugText = shapeClass("DebugText", DebugShape, TYPES.Text, TEXT_DEFAULTS, textFields);
    const textMembers = (cls, name) => {
        accessor(cls, name, "backfaceVisible", (s) => s.backfaceVisible,
            (s, value) => { s.backfaceVisible = bool(value); });
        accessor(cls, name, "backgroundColorOverride",
            (s) => (s.backgroundColorOverride === undefined ? undefined : Object.assign({}, s.backgroundColorOverride)),
            (s, value) => { s.backgroundColorOverride = value === undefined ? undefined : rgba(value); });
        accessor(cls, name, "depthTest", (s) => s.depthTest, (s, value) => { s.depthTest = bool(value); });
        accessor(cls, name, "lineGapHeight", (s) => s.lineGapHeight,
            (s, value) => { s.lineGapHeight = number(value); });
        accessor(cls, name, "text", (s) => copyText(s.text));
        accessor(cls, name, "textBackfaceVisible", (s) => s.textBackfaceVisible,
            (s, value) => { s.textBackfaceVisible = bool(value); });
        accessor(cls, name, "useRotation", (s) => s.useRotation,
            (s, value) => { s.useRotation = bool(value); });
        method(cls, name, "setText", (s, self, text) => {
            s.wire = textOf(text);
            s.text = copyText(text);
            s.dirty = true;
        });
    };
    textMembers(DebugText, "DebugText");

    // the server module's text label (sub_14B8D4360), drawn by the same stream as the debug shapes
    const PrimitiveShape = baseClass("PrimitiveShape", false);
    const TextPrimitive = shapeClass("TextPrimitive", PrimitiveShape, TYPES.Text, TEXT_DEFAULTS, textFields);
    textMembers(TextPrimitive, "TextPrimitive");

    const isShape = (value) => value !== null && typeof value === "object" && states.has(value);

    class DebugDrawer {
        constructor(key) {
            if (key !== token)
                throw new ReferenceError("No constructor for native class 'DebugDrawer'.");
        }
    }
    named(DebugDrawer, "DebugDrawer");
    const drawerMethod = (name, run) => {
        Object.defineProperty(DebugDrawer.prototype, name, { configurable: true, writable: true, enumerable: false,
            value: function (...args) {
                guard("Native function", "DebugDrawer::" + name);
                return run(...args);
            } });
    };
    // each drawer keeps to its own module's shapes
    const isDebugShape = (value) => isShape(value) && value instanceof DebugShape;
    drawerMethod("addShape", (shape, dimension) => {
        if (!isDebugShape(shape))
            throw conversion();
        const state = states.get(shape);
        if (dimension !== undefined) {
            if (native.dimensionId(dimension) < 0)
                throw conversion();
            state.dimension = dimension;
        }
        if (state.id === 0)
            state.id = native.nextShapeId();
        state.dirty = true;
        drawn.add(state);
    });
    drawerMethod("removeAll", () => {
        for (const state of Array.from(drawn).filter((entry) => !entry.server))
            removeShape(state);
    });
    drawerMethod("removeShape", (shape) => {
        if (!isDebugShape(shape))
            throw conversion();
        removeShape(states.get(shape));
    });
    const debugDrawer = new DebugDrawer(token);

    // PrimitiveShapesManager (sub_14B966010 and its siblings, 26.60): only its own text shapes, at most 1000 of them
    const MAX_SHAPES = 1000;
    class PrimitiveShapesManager {
        constructor(key) {
            if (key !== token)
                throw new ReferenceError("No constructor for native class 'PrimitiveShapesManager'.");
        }
    }
    named(PrimitiveShapesManager, "PrimitiveShapesManager");
    const managerMethod = (name, run) => {
        Object.defineProperty(PrimitiveShapesManager.prototype, name, { configurable: true, writable: true,
            enumerable: false, value: function (...args) {
                guard("Native function", "PrimitiveShapesManager::" + name);
                return run(...args);
            } });
    };
    Object.defineProperty(PrimitiveShapesManager.prototype, "maxShapes", { configurable: true, enumerable: false,
        get: function () {
            guard("Native property getter", "PrimitiveShapesManager::maxShapes");
            return MAX_SHAPES;
        } });
    const isText = (value) => isShape(value) && value instanceof TextPrimitive;
    const managed = () => Array.from(drawn).filter((state) => state.server);
    // a shape already in the manager is left as is; a new one needs room
    managerMethod("addText", (text, dimension) => {
        if (!isText(text))
            throw conversion();
        if (dimension !== undefined && native.dimensionId(dimension) < 0)
            throw conversion();
        const state = states.get(text);
        if (drawn.has(state) && state.server)
            return;
        if (managed().length >= MAX_SHAPES)
            throw native.errors.make("PrimitiveShapeError",
                "PrimitiveShapesManager cannot add more shapes, limit reached: " + MAX_SHAPES);
        if (dimension !== undefined)
            state.dimension = dimension;
        if (state.id === 0)
            state.id = native.nextShapeId();
        state.server = true;
        state.dirty = true;
        drawn.add(state);
    });
    managerMethod("removeAll", () => {
        for (const state of managed())
            removeShape(state);
    });
    managerMethod("removeText", (text) => {
        if (!isText(text))
            throw conversion();
        removeShape(states.get(text));
    });
    // sub_14B96CE70: attachedTo must match, and with a location the shape lies within [minDistance, maxDistance]
    managerMethod("getShapes", (options) => {
        if (options !== undefined && (options === null || typeof options !== "object"))
            throw conversion();
        const query = options === undefined ? {} : options;
        if (query.attachedTo !== undefined && !native.isEntity(query.attachedTo))
            throw conversion();
        const location = query.location === undefined ? undefined : vector3(query.location);
        const minDistance = query.minDistance === undefined ? undefined : number(query.minDistance);
        const maxDistance = query.maxDistance === undefined ? undefined : number(query.maxDistance);
        const attachedId = query.attachedTo === undefined ? undefined : native.entityId(query.attachedTo);
        return managed().filter((state) => {
            if (query.attachedTo !== undefined
                && (state.attachedTo === undefined || native.entityId(state.attachedTo) !== attachedId))
                return false;
            if (location !== undefined) {
                const dx = state.location.x - location.x, dy = state.location.y - location.y,
                    dz = state.location.z - location.z;
                const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
                if ((minDistance !== undefined && minDistance > distance)
                    || (maxDistance !== undefined && distance > maxDistance))
                    return false;
            }
            return true;
        }).map((state) => state.shape);
    });
    const primitiveShapesManager = new PrimitiveShapesManager(token);

    // DiagnosticsManager: tabs and views of this pack for the script debugger's live diagnostics
    const diagnosticsError = (reason, message) => native.errors.make("DiagnosticsError", message, { reason });
    const tabStates = new WeakMap();
    const viewStates = new WeakMap();
    const tabs = [];
    const tabOf = (value) => {
        const state = value !== null && typeof value === "object" ? tabStates.get(value) : undefined;
        if (state === undefined)
            throw conversion();
        return state;
    };
    const isView = (value) => value !== null && typeof value === "object" && viewStates.has(value);
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
    const define = (cls, className, name, run, getter) => {
        const label = getter ? "Native property getter" : "Native function";
        const wrapped = function (...args) {
            guard(label, className + "::" + name);
            return run(this, ...args);
        };
        Object.defineProperty(cls.prototype, name, getter
            ? { configurable: true, enumerable: false, get: wrapped }
            : { configurable: true, writable: true, enumerable: false, value: wrapped });
    };

    const DiagnosticsView = privateClass("DiagnosticsView");
    define(DiagnosticsView, "DiagnosticsView", "pushStats", (self, stats) => {
        const state = viewStates.get(self);
        if (state === undefined)
            throw unbound("Native function", "DiagnosticsView::pushStats");
        if (!Array.isArray(stats) || !stats.every((stat) => stat !== null && typeof stat === "object"
            && typeof stat.name === "string"
            && (stat.values === undefined || (Array.isArray(stat.values)
                && stat.values.every((value) => typeof value === "number" || typeof value === "string")))))
            throw conversion();
        if (state.pushedAt === frame)
            throw diagnosticsError("InvalidData", "Stats have already been pushed this frame!");
        state.pushedAt = frame;
        state.stats = stats.map((stat) => ({ name: stat.name, values: stat.values === undefined ? undefined
            : stat.values.slice() }));
    });

    const DiagnosticsTab = privateClass("DiagnosticsTab");
    define(DiagnosticsTab, "DiagnosticsTab", "tabName", (self) => tabOf(self).name, true);
    define(DiagnosticsTab, "DiagnosticsTab", "views", (self) => tabOf(self).views.slice(), true);
    define(DiagnosticsTab, "DiagnosticsTab", "addView", (self, view) => {
        const state = tabOf(self);
        if (!isView(view))
            throw conversion();
        if (!state.views.includes(view))
            state.views.push(view);
    });
    define(DiagnosticsTab, "DiagnosticsTab", "containsView", (self, view) => {
        if (!isView(view))
            throw conversion();
        return tabOf(self).views.includes(view);
    });
    define(DiagnosticsTab, "DiagnosticsTab", "removeView", (self, view) => {
        const state = tabOf(self);
        if (!isView(view))
            throw conversion();
        state.views = state.views.filter((other) => other !== view);
    });

    const CHART = [0, 1, 2];
    const TABLE = [0, 1];
    const DiagnosticsManager = privateClass("DiagnosticsManager");
    define(DiagnosticsManager, "DiagnosticsManager", "tabs", () => tabs.slice(), true);
    define(DiagnosticsManager, "DiagnosticsManager", "addTab", (self, tab) => {
        tabOf(tab);
        if (!tabs.includes(tab))
            tabs.push(tab);
    });
    define(DiagnosticsManager, "DiagnosticsManager", "containsTab", (self, tab) => {
        tabOf(tab);
        return tabs.includes(tab);
    });
    define(DiagnosticsManager, "DiagnosticsManager", "containsView", (self, view) => {
        if (!isView(view))
            throw conversion();
        return tabs.some((tab) => tabStates.get(tab).views.includes(view));
    });
    define(DiagnosticsManager, "DiagnosticsManager", "createTab", (self, tabName) => {
        if (typeof tabName !== "string")
            throw conversion();
        if (tabName.length === 0)
            throw diagnosticsError("InvalidName", "tabName must be non-empty");
        if (tabs.some((tab) => tabStates.get(tab).name === tabName))
            throw diagnosticsError("AlreadyRegistered", "Tab with the name '" + tabName + "' already exists!");
        const tab = new DiagnosticsTab(token);
        tabStates.set(tab, { name: tabName, views: [] });
        tabs.push(tab);
        return tab;
    });
    define(DiagnosticsManager, "DiagnosticsManager", "createView", (self, statName, options) => {
        if (typeof statName !== "string")
            throw conversion();
        if (options !== undefined) {
            if (options === null || typeof options !== "object")
                throw conversion();
            const chart = options.chartType !== undefined;
            const kind = chart ? options.chartType : options.tableType;
            if (!(chart ? CHART : TABLE).includes(kind))
                throw conversion();
        }
        if (statName.length === 0)
            throw diagnosticsError("InvalidName", "statName must be non-empty");
        const view = new DiagnosticsView(token);
        viewStates.set(view, { name: statName, options, pushedAt: undefined, stats: [] });
        return view;
    });
    define(DiagnosticsManager, "DiagnosticsManager", "removeTab", (self, tab) => {
        tabOf(tab);
        const index = tabs.indexOf(tab);
        if (index >= 0)
            tabs.splice(index, 1);
    });
    const diagnosticsManager = new DiagnosticsManager(token);

    const fn = (name, run) => function (...args) {
        guard("Native function", name);
        return run(...args);
    };

    // visibleTo wins over defaultVisibleToAll, hiddenFrom over both
    const viewersOf = (state, online) => {
        const base = state.visibleTo.length > 0 ? online.filter((player) => state.visibleTo.includes(player))
            : (state.defaultVisibleToAll ? online : []);
        return base.filter((player) => !state.hiddenFrom.includes(player));
    };
    const snapshotOf = (state) => {
        const vec = (v) => [v.x, v.y, v.z];
        const color = (c) => [c.red, c.green, c.blue, c.alpha];
        const snapshot = { id: state.id, type: state.type, location: vec(state.location),
            dimension: state.dimension === undefined ? 3 : native.dimensionId(state.dimension),
            scale: state.scale, rotation: vec(state.rotation),
            timeLeft: state.timeLeft === undefined ? 0 : state.timeLeft,
            maximumRenderDistance: state.maximumRenderDistance === undefined ? -1 : state.maximumRenderDistance,
            color: color(state.color) };
        if (state.attachedTo !== undefined) {
            const id = native.entityId(state.attachedTo);
            if (id !== undefined)
                snapshot.attachedTo = id;
        }
        switch (state.type) {
            case TYPES.Line:
            case TYPES.Arrow:
                snapshot.endLocation = vec(state.endLocation);
                snapshot.headLength = state.headLength;
                snapshot.headRadius = state.headRadius;
                snapshot.headSegments = state.headSegments;
                break;
            case TYPES.Box:
                snapshot.bound = vec(state.bound);
                break;
            case TYPES.Text:
                snapshot.text = state.wire;
                snapshot.useRotation = state.useRotation;
                if (state.backgroundColorOverride !== undefined)
                    snapshot.backgroundColor = color(state.backgroundColorOverride);
                snapshot.lineGapHeight = state.lineGapHeight;
                snapshot.depthTest = state.depthTest;
                snapshot.backfaceVisible = state.backfaceVisible;
                snapshot.textBackfaceVisible = state.textBackfaceVisible;
                break;
            case TYPES.Ellipsoid:
                snapshot.radii = vec(state.radii3);
                snapshot.segments = state.segments;
                break;
            case TYPES.Pyramid:
                snapshot.width = state.width;
                if (state.depth !== undefined)
                    snapshot.depth = state.depth;
                snapshot.height = state.height;
                break;
            default:
                snapshot.segments = state.segments;
                if (state.radii !== undefined) {
                    snapshot.radii = [state.radii.x, state.radii.y];
                    snapshot.height = state.height;
                }
        }
        return snapshot;
    };
    // once per tick: expire timed shapes, then send each player what it newly sees, what changed and what left
    const tick = () => {
        ++frame;
        for (const state of Array.from(drawn)) {
            if (state.timeLeft === undefined)
                continue;
            state.timeLeft -= 0.05;
            if (state.timeLeft <= 0) {
                state.timeLeft = 0;
                removeShape(state);
            }
        }
        const online = native.players();
        const outgoing = new Map();
        const queue = (player, snapshot) => {
            if (!outgoing.has(player))
                outgoing.set(player, []);
            outgoing.get(player).push(snapshot);
        };
        for (const [player, id] of removals.splice(0))
            if (online.includes(player))
                queue(player, { id });
        for (const state of drawn) {
            const viewers = new Set(viewersOf(state, online));
            let snapshot;
            for (const player of viewers) {
                if (state.dirty || !state.viewers.has(player))
                    queue(player, snapshot === undefined ? (snapshot = snapshotOf(state)) : snapshot);
            }
            for (const player of state.viewers)
                if (!viewers.has(player) && online.includes(player))
                    queue(player, { id: state.id });
            state.viewers = viewers;
            state.dirty = false;
        }
        for (const [player, shapes] of outgoing)
            native.send(player, shapes);
    };

    return {
        DebugArrow, DebugBox, DebugCircle, DebugCone, DebugCylinder, DebugDrawer, DebugEllipsoid, DebugLine,
        DebugPyramid, DebugShape, DebugSphere, DebugText, DiagnosticsManager, DiagnosticsTab, DiagnosticsView,
        collectPluginStats: fn("collectPluginStats", () => native.pluginStats()),
        collectRuntimeStats: fn("collectRuntimeStats", () => native.runtimeStats()),
        disableWatchdogTimingWarnings: fn("disableWatchdogTimingWarnings", (disable) => native.timingWarnings(disable)),
        debugDrawer, diagnosticsManager, tick, PrimitiveShape, PrimitiveShapesManager, TextPrimitive,
        primitiveShapesManager,
    };
})
)JS";
}

namespace ScriptDebugBootstrap {
    const char *source() {
        return SOURCE;
    }
}
