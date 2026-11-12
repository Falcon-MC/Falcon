#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <random>

using namespace ScriptValues;

namespace {
    ScriptStructureRegistry &registryOf(ScriptApi &api) {
        ScriptStructureRegistry &registry = api.host().getScriptEngine().structures();
        // world structures go to the default world, like the game's single level database
        registry.setStorage(&api.host().getWorlds().getDefault().getOverworld());
        return registry;
    }

    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    bool readInts(JSContext *ctx, JSValueConst value, Vector3i &out) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, value, x, y, z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        out = Vector3i((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        return true;
    }

    // a Structure or its identifier
    bool readStructureId(ScriptApi &api, JSValueConst value, std::string &identifier) {
        if (JS_IsString(value)) {
            identifier = ScriptStructureRegistry::normalize(toString(api.context(), value));
            return true;
        }
        if (const ScriptStructureRef *ref = api.native<ScriptStructureRef>(value)) {
            identifier = ref->mId;
            return true;
        }
        return false;
    }

    // undefined stays Memory
    bool readSaveMode(JSContext *ctx, JSValueConst value, bool &toWorld) {
        toWorld = false;
        if (JS_IsUndefined(value))
            return true;
        const std::string mode = toString(ctx, value);
        if (mode != "Memory" && mode != "World")
            return false;
        toWorld = mode == "World";
        return true;
    }

    JSValue optionValue(JSContext *ctx, JSValueConst options, const char *key) {
        return JS_IsObject(options) ? JS_GetPropertyStr(ctx, options, key) : JS_UNDEFINED;
    }

    bool optionBool(JSContext *ctx, JSValueConst options, const char *key, bool fallback) {
        JSValue value = optionValue(ctx, options, key);
        const bool result = JS_IsUndefined(value) ? fallback : JS_ToBool(ctx, value) == 1;
        JS_FreeValue(ctx, value);
        return result;
    }

    JSValue invalidIdentifier(ScriptApi &api, const std::string &identifier) {
        return api.throwError("Error", "Invalid structure identifier " + identifier);
    }

    JSValue managerCreateEmpty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i size;
        bool toWorld = false;
        if (argc < 2 || !JS_IsString(argv[0]) || !readInts(ctx, argv[1], size)
            || !readSaveMode(ctx, argc >= 3 ? argv[2] : JS_UNDEFINED, toWorld))
            return typeError(ctx);
        const std::string raw = toString(ctx, argv[0]);
        if (!ScriptStructureRegistry::isValidIdentifier(raw))
            return invalidIdentifier(*api, raw);
        if (size.x < 1 || size.y < 1 || size.z < 1)
            return api->throwError("InvalidArgumentError", "Structure size must be at least 1 on each axis.");
        const std::string id = ScriptStructureRegistry::normalize(raw);
        if (!registryOf(*api).add(id, std::make_shared<McStructure>(size), toWorld))
            return api->throwError("Error", "Structure " + id + " already exists.");
        return api->makeStructure(id);
    }

    JSValue managerCreateFromWorld(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i from, to;
        if (argc < 4 || !JS_IsString(argv[0]) || api->native<ScriptDimensionRef>(argv[1]) == nullptr
            || !readInts(ctx, argv[2], from) || !readInts(ctx, argv[3], to))
            return typeError(ctx);
        JSValueConst options = argc >= 5 ? argv[4] : JS_UNDEFINED;
        bool toWorld = false;
        JSValue mode = optionValue(ctx, options, "saveMode");
        const bool validMode = readSaveMode(ctx, mode, toWorld);
        JS_FreeValue(ctx, mode);
        if (!validMode)
            return typeError(ctx);
        const std::string raw = toString(ctx, argv[0]);
        if (!ScriptStructureRegistry::isValidIdentifier(raw))
            return invalidIdentifier(*api, raw);

        Level &level = api->levelOf(argv[1]);
        for (const Vector3i &corner: {from, to}) {
            if (corner.y < level.getMinY() || corner.y > level.getMaxY())
                return api->throwError("LocationOutOfWorldBoundariesError", "");
        }
        const std::string id = ScriptStructureRegistry::normalize(raw);
        if (registryOf(*api).exists(id))
            return api->throwError("Error", "Structure " + id + " already exists.");
        std::shared_ptr<McStructure> structure = std::make_shared<McStructure>(
                McStructure::capture(api->host(), level, from, to, optionBool(ctx, options, "includeBlocks", true),
                                     optionBool(ctx, options, "includeEntities", true)));
        registryOf(*api).add(id, structure, toWorld);
        return api->makeStructure(id);
    }

    JSValue managerDelete(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        std::string id;
        if (argc < 1 || !readStructureId(*api, argv[0], id))
            return typeError(ctx);
        return JS_NewBool(ctx, registryOf(*api).remove(id));
    }

    JSValue managerGet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return typeError(ctx);
        const std::string id = ScriptStructureRegistry::normalize(toString(ctx, argv[0]));
        return registryOf(*api).exists(id) ? api->makeStructure(id) : JS_UNDEFINED;
    }

    JSValue stringArray(JSContext *ctx, const std::vector<std::string> &values) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &value: values)
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, value));
        return array;
    }

    JSValue managerGetIds(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic) {
        ScriptStructureRegistry &registry = registryOf(*ScriptApi::fromContext(ctx));
        return stringArray(ctx, magic == 0 ? registry.packIds() : registry.worldIds());
    }

    bool readRotation(const std::string &value, int32_t &rotation) {
        static const char *const NAMES[] = {"None", "Rotate90", "Rotate180", "Rotate270"};
        for (int32_t index = 0; index < 4; ++index) {
            if (value == NAMES[index]) {
                rotation = index;
                return true;
            }
        }
        return false;
    }

    bool readMirror(const std::string &value, McStructure::Mirror &mirror) {
        if (value == "None")
            mirror = McStructure::Mirror::None;
        else if (value == "X")
            mirror = McStructure::Mirror::X;
        else if (value == "Z")
            mirror = McStructure::Mirror::Z;
        else if (value == "XZ")
            mirror = McStructure::Mirror::XZ;
        else
            return false;
        return true;
    }

    // sub_141EC6740: trimmed, a whole base 10 integer as is, else a 31 hash over the code points; empty reads none
    bool readIntegritySeed(const std::string &text, uint32_t &seed) {
        const char *const blanks = "\t\n\v\f\r ";
        const size_t first = text.find_first_not_of(blanks);
        if (first == std::string::npos)
            return false;
        const std::string trimmed = text.substr(first, text.find_last_not_of(blanks) - first + 1);
        errno = 0;
        char *end = nullptr;
        const long long value = std::strtoll(trimmed.c_str(), &end, 10);
        if (end == trimmed.c_str() + trimmed.size() && errno != ERANGE) {
            seed = (uint32_t) value;
            return true;
        }
        int32_t hash = 0;
        for (size_t at = 0; at < trimmed.size();) {
            const unsigned char lead = (unsigned char) trimmed[at];
            const size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            uint32_t point = length == 1 ? lead : lead & (0x7F >> length);
            for (size_t index = 1; index < length && at + index < trimmed.size(); ++index)
                point = (point << 6) | ((unsigned char) trimmed[at + index] & 0x3F);
            hash = (int32_t) ((uint32_t) point + 31u * (uint32_t) hash);
            at += length;
        }
        seed = (uint32_t) hash;
        return true;
    }

    JSValue managerPlace(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        std::string id;
        Vector3i at;
        if (argc < 3 || !readStructureId(*api, argv[0], id) || api->native<ScriptDimensionRef>(argv[1]) == nullptr
            || !readInts(ctx, argv[2], at))
            return typeError(ctx);
        std::shared_ptr<McStructure> structure = registryOf(*api).find(id);
        if (structure == nullptr)
            return api->throwError("InvalidStructureError", "Structure " + id + " does not exist.");

        JSValueConst options = argc >= 4 ? argv[3] : JS_UNDEFINED;
        McStructure::PlaceOptions place;
        place.mBlocks = optionBool(ctx, options, "includeBlocks", true);
        place.mEntities = optionBool(ctx, options, "includeEntities", true);
        place.mWaterlogged = optionBool(ctx, options, "waterlogged", false);
        JSValue rotation = optionValue(ctx, options, "rotation");
        JSValue mirror = optionValue(ctx, options, "mirror");
        JSValue integrity = optionValue(ctx, options, "integrity");
        JSValue seed = optionValue(ctx, options, "integritySeed");
        bool valid = (JS_IsUndefined(rotation) || readRotation(toString(ctx, rotation), place.mRotation))
                     && (JS_IsUndefined(mirror) || readMirror(toString(ctx, mirror), place.mMirror));
        double share = 1.0;
        if (valid && !JS_IsUndefined(integrity))
            valid = JS_ToFloat64(ctx, &share, integrity) == 0 && std::isfinite(share);
        const bool hasSeed = !JS_IsUndefined(seed);
        const std::string seedText = hasSeed ? toString(ctx, seed) : std::string();
        for (JSValue value: {rotation, mirror, integrity, seed})
            JS_FreeValue(ctx, value);
        if (!valid)
            return typeError(ctx);
        if (share < 0.0 || share > 1.0)
            return api->throwError("ArgumentOutOfBoundsError", "");
        if (hasSeed && !readIntegritySeed(seedText, place.mSeed))
            return api->throwError("InvalidArgumentError", "Invalid integrity seed.");
        place.mIntegrity = (float) share;

        Level &level = api->levelOf(argv[1]);
        const Vector3i size = structure->rotatedSize(place.mRotation);
        if (at.y < level.getMinY() || at.y + size.y - 1 > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        JSValue mode = optionValue(ctx, options, "animationMode");
        const std::string animation = JS_IsString(mode) ? toString(ctx, mode) : std::string("None");
        JS_FreeValue(ctx, mode);
        JSValue secondsValue = optionValue(ctx, options, "animationSeconds");
        double seconds = 0.0;
        if (!JS_IsUndefined(secondsValue) && JS_ToFloat64(ctx, &seconds, secondsValue) != 0)
            seconds = 0.0;
        JS_FreeValue(ctx, secondsValue);
        const int64_t ticks = (int64_t) std::ceil(seconds * 20.0);
        if (animation == "None" || ticks <= 0 || !place.mBlocks) {
            structure->place(api->host(), level, at, place);
            return JS_UNDEFINED;
        }

        // Layers steps whole y layers; Blocks steps single cells
        const Vector3i &raw = structure->getSize();
        const int64_t layer = (int64_t) raw.x * raw.z;
        const int64_t units = animation == "Layers" ? (int64_t) raw.y : layer * raw.y;
        const int64_t unitCells = animation == "Layers" ? layer : 1;
        auto step = std::make_shared<int64_t>(0);
        ServerNetworkHandler *owner = &api->host();
        Level *target = &level;
        api->addTickJob([structure, place, owner, target, at, ticks, units, unitCells, step]() {
            const int64_t done = (*step)++;
            McStructure::PlaceOptions part = place;
            part.mFromCell = (units * done / ticks) * unitCells;
            part.mToCell = (units * (done + 1) / ticks) * unitCells;
            part.mEntities = place.mEntities && done + 1 == ticks;
            structure->place(*owner, *target, at, part);
            return done + 1 < ticks;
        });
        return JS_UNDEFINED;
    }

    JSValue boundingBox(JSContext *ctx, const PackJigsaw::Box &box) {
        JSValue value = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, value, "min", vector3(ctx, (float) box.mMin.x, (float) box.mMin.y, (float) box.mMin.z));
        JS_SetPropertyStr(ctx, value, "max", vector3(ctx, (float) box.mMax.x, (float) box.mMax.y, (float) box.mMax.z));
        return value;
    }

    PackJigsaw::Options jigsawOptions(JSContext *ctx, JSValueConst options) {
        PackJigsaw::Options read;
        read.mIncludeEntities = optionBool(ctx, options, "includeEntities", true);
        read.mKeepJigsaws = optionBool(ctx, options, "keepJigsaws", false);
        JSValue liquid = optionValue(ctx, options, "liquidSettings");
        if (JS_IsString(liquid))
            read.mApplyWaterlogging = toString(ctx, liquid) != "IgnoreWaterlogging";
        JS_FreeValue(ctx, liquid);
        return read;
    }

    int64_t jigsawSeed() {
        return (int64_t) std::chrono::steady_clock::now().time_since_epoch().count();
    }

    JSValue runJigsaw(ScriptApi &api, Level &level, const std::string &pool, const std::string &jigsaw, int32_t depth,
                      const Vector3i &at, const PackJigsaw::Options &options) {
        ScriptStructureRegistry &registry = registryOf(api);
        PackJigsaw::Box box;
        std::string error;
        const bool placed = registry.jigsaw().place(
                api.host(), level, pool, jigsaw, depth, at, options,
                [&](const std::string &id) { return registry.find(id); }, jigsawSeed(), box, error);
        // sub_14B7F2440: the game wraps the placer's message in PlaceJigsawError
        if (!placed)
            return api.throwError("PlaceJigsawError", error);
        return boundingBox(api.context(), box);
    }

    // maxDepth spans 1 to 20, from the definitions
    JSValue managerPlaceJigsaw(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        int32_t depth = 0;
        Vector3i at;
        if (argc < 5 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]) || JS_ToInt32(ctx, &depth, argv[2]) != 0
            || api->native<ScriptDimensionRef>(argv[3]) == nullptr || !readInts(ctx, argv[4], at))
            return typeError(ctx);
        if (depth < 1 || depth > 20)
            return api->throwError("ArgumentOutOfBoundsError", "");
        return runJigsaw(*api, api->levelOf(argv[3]), toString(ctx, argv[0]), toString(ctx, argv[1]), depth, at,
                         jigsawOptions(ctx, argc >= 6 ? argv[5] : JS_UNDEFINED));
    }

    JSValue managerPlaceJigsawStructure(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        if (argc < 3 || !JS_IsString(argv[0]) || api->native<ScriptDimensionRef>(argv[1]) == nullptr
            || !readInts(ctx, argv[2], at))
            return typeError(ctx);
        const std::string id = toString(ctx, argv[0]);
        const PackJigsaw::Structure *structure = registryOf(*api).jigsaw().structure(id);
        // commands.place.invalidstructure.error (sub_14BEF2320)
        if (structure == nullptr)
            return api->throwError("PlaceJigsawError", "Invalid structure name.");
        JSValueConst options = argc >= 4 ? argv[3] : JS_UNDEFINED;
        Level &level = api->levelOf(argv[1]);
        if (structure->mHasStartHeight && !optionBool(ctx, options, "ignoreStartHeight", false))
            at.y = PackJigsaw::startHeight(*structure, level.getSeed(), at, level.getMinY(), level.getMaxY());
        return runJigsaw(*api, level, structure->mStartPool, std::string(), structure->mMaxDepth, at,
                         jigsawOptions(ctx, options));
    }

    // a structure deleted under the object answers InvalidStructureError
    std::shared_ptr<McStructure> structureOf(JSContext *ctx, JSValueConst thisVal, std::string &id) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        id = api->native<ScriptStructureRef>(thisVal)->mId;
        return registryOf(*api).find(id);
    }

    JSValue invalidStructure(JSContext *ctx, const std::string &id) {
        return ScriptApi::fromContext(ctx)->throwError("InvalidStructureError", "Structure " + id + " is invalid.");
    }

    JSValue structureGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, ScriptApi::fromContext(ctx)->native<ScriptStructureRef>(thisVal)->mId);
    }

    JSValue structureGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::string id;
        return JS_NewBool(ctx, structureOf(ctx, thisVal, id) != nullptr);
    }

    JSValue structureGetSize(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::string id;
        std::shared_ptr<McStructure> structure = structureOf(ctx, thisVal, id);
        if (structure == nullptr)
            return invalidStructure(ctx, id);
        const Vector3i &size = structure->getSize();
        return vector3(ctx, (float) size.x, (float) size.y, (float) size.z);
    }

    // a location inside the structure, else ArgumentOutOfBoundsError
    bool readCell(JSContext *ctx, const McStructure &structure, JSValueConst value, Vector3i &cell, JSValue &error) {
        error = JS_UNDEFINED;
        if (!readInts(ctx, value, cell)) {
            error = typeError(ctx);
            return false;
        }
        if (!structure.contains(cell)) {
            error = ScriptApi::fromContext(ctx)->throwError("ArgumentOutOfBoundsError", "");
            return false;
        }
        return true;
    }

    JSValue structureGetBlockPermutation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string id;
        std::shared_ptr<McStructure> structure = structureOf(ctx, thisVal, id);
        if (structure == nullptr)
            return invalidStructure(ctx, id);
        Vector3i cell;
        JSValue error;
        if (argc < 1 || !readCell(ctx, *structure, argv[0], cell, error))
            return argc < 1 ? typeError(ctx) : error;
        const BlockState *state = structure->getBlock(cell);
        return state == nullptr ? JS_UNDEFINED : ScriptApi::fromContext(ctx)->makePermutation(*state);
    }

    JSValue structureGetIsWaterlogged(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string id;
        std::shared_ptr<McStructure> structure = structureOf(ctx, thisVal, id);
        if (structure == nullptr)
            return invalidStructure(ctx, id);
        Vector3i cell;
        JSValue error;
        if (argc < 1 || !readCell(ctx, *structure, argv[0], cell, error))
            return argc < 1 ? typeError(ctx) : error;
        return JS_NewBool(ctx, structure->isWaterlogged(cell));
    }

    JSValue structureSetBlockPermutation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        std::string id;
        std::shared_ptr<McStructure> structure = structureOf(ctx, thisVal, id);
        if (structure == nullptr)
            return invalidStructure(ctx, id);
        Vector3i cell;
        JSValue error;
        if (argc < 1 || !readCell(ctx, *structure, argv[0], cell, error))
            return argc < 1 ? typeError(ctx) : error;
        if (argc < 2 || JS_IsUndefined(argv[1])) {
            structure->setBlock(cell, nullptr);
            structure->setBlock(cell, nullptr, 1);
            return JS_UNDEFINED;
        }
        BlockState state;
        if (!api->readPermutation(argv[1], state))
            return typeError(ctx);
        structure->setBlock(cell, &state);
        const bool waterlogged = argc >= 3 && JS_ToBool(ctx, argv[2]) == 1;
        BlockState water("minecraft:water");
        water.mStates.putInt("liquid_depth", 0);
        structure->setBlock(cell, waterlogged ? &water : nullptr, 1);
        return JS_UNDEFINED;
    }

    JSValue structureSaveAs(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        std::string id;
        std::shared_ptr<McStructure> structure = structureOf(ctx, thisVal, id);
        if (structure == nullptr)
            return invalidStructure(ctx, id);
        bool toWorld = false;
        if (argc < 1 || !JS_IsString(argv[0]) || !readSaveMode(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, toWorld))
            return typeError(ctx);
        const std::string raw = toString(ctx, argv[0]);
        if (!ScriptStructureRegistry::isValidIdentifier(raw))
            return invalidIdentifier(*api, raw);
        const std::string copy = ScriptStructureRegistry::normalize(raw);
        if (!registryOf(*api).add(copy, std::make_shared<McStructure>(*structure), toWorld))
            return api->throwError("Error", "Structure " + copy + " already exists.");
        return api->makeStructure(copy);
    }

    JSValue structureSaveToWorld(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::string id;
        if (structureOf(ctx, thisVal, id) == nullptr)
            return invalidStructure(ctx, id);
        registryOf(*ScriptApi::fromContext(ctx)).saveToWorld(id);
        return JS_UNDEFINED;
    }
}

JSValue ScriptApi::makeStructure(const std::string &identifier) {
    ScriptStructureRef *ref = new ScriptStructureRef();
    ref->mId = identifier;
    return makeInstance("Structure", ref);
}

namespace {
    JSValue structureManagerGetter(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_DupValue(ctx, ScriptApi::fromContext(ctx)->structureManager());
    }
}

void ScriptApi::_bindStructures() {
    if (!mSurface.has("StructureManager") || !mSurface.has("Structure"))
        return;
    JSValue structure = defineClass("Structure");
    property(structure, "Structure", "id", structureGetId);
    property(structure, "Structure", "isValid", structureGetIsValid);
    property(structure, "Structure", "size", structureGetSize);
    method(structure, "Structure", "getBlockPermutation", structureGetBlockPermutation, 1);
    method(structure, "Structure", "getIsWaterlogged", structureGetIsWaterlogged, 1);
    method(structure, "Structure", "setBlockPermutation", structureSetBlockPermutation, 3);
    method(structure, "Structure", "saveAs", structureSaveAs, 2);
    method(structure, "Structure", "saveToWorld", structureSaveToWorld, 0);

    JSValue manager = defineClass("StructureManager");
    method(manager, "StructureManager", "createEmpty", managerCreateEmpty, 3);
    method(manager, "StructureManager", "createFromWorld", managerCreateFromWorld, 5);
    method(manager, "StructureManager", "delete", managerDelete, 1);
    method(manager, "StructureManager", "get", managerGet, 1);
    methodMagic(manager, "StructureManager", "getPackStructureIds", managerGetIds, 0, 0);
    methodMagic(manager, "StructureManager", "getWorldStructureIds", managerGetIds, 0, 1);
    method(manager, "StructureManager", "place", managerPlace, 4);
    method(manager, "StructureManager", "placeJigsaw", managerPlaceJigsaw, 6);
    method(manager, "StructureManager", "placeJigsawStructure", managerPlaceJigsawStructure, 4);
    mStructureManager = makeInstance("StructureManager", new ScriptNative());
    property(mWorld, "World", "structureManager", structureManagerGetter);
}
