#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Level.h"
#include "World/Village/PoiBlockTypes.h"
#include "World/Village/PoiManager.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ScriptValues;

namespace {
    // the Poi experiment's level.dat toggle
    const char *const POI_EXPERIMENT = "epoi";

    // PoiManager and PoiBlockManager: one dimension
    struct ScriptPoiDimension : ScriptNative {
        uint32_t mWorldId = 0;
        DimensionType mDimension = DimensionType::Overworld;
    };

    struct ScriptPoiType : ScriptNative {
        uint16_t mId = 0;
    };

    // a copy of the instance when the query ran, like ScriptInstance
    struct ScriptPoiInstance : ScriptNative {
        Vector3i mPosition;
        uint16_t mType = 0;
        uint8_t mTickets = 0;
    };

    enum class Occupancy { Any = 0, Full = 1, HasVacancy = 2 };

    struct TypeFilter {
        enum class Kind { Callback, Name, Tags } mKind = Kind::Name;
        JSValueConst mCallback = JS_UNDEFINED;
        std::string mName;
        std::vector<std::string> mTags;
    };

    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    ScriptApi *apiOf(JSContext *ctx) {
        return ScriptApi::fromContext(ctx);
    }

    JSValue makeType(ScriptApi &api, uint16_t id) {
        ScriptPoiType *type = new ScriptPoiType();
        type->mId = id;
        return api.makeInstance("PoiBlockType", type);
    }

    JSValue makePoiInstance(ScriptApi &api, const PoiManager::Entry &entry) {
        ScriptPoiInstance *instance = new ScriptPoiInstance();
        instance->mPosition = entry.mPosition;
        instance->mType = entry.mType;
        instance->mTickets = entry.mTickets;
        return api.makeInstance("PoiBlockInstance", instance);
    }

    JSValue blockPosition(JSContext *ctx, const Vector3i &position) {
        return vector3(ctx, (float) position.x, (float) position.y, (float) position.z);
    }

    bool readBlockPosition(JSContext *ctx, JSValueConst value, Vector3i &out) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, value, x, y, z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        out = Vector3i((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        return true;
    }

    // the radius arrives as an unsigned short
    bool readRadius(JSContext *ctx, JSValueConst value, uint16_t &out) {
        int32_t radius = 0;
        if (!JS_IsNumber(value) || JS_ToInt32(ctx, &radius, value) != 0)
            return false;
        out = (uint16_t) radius;
        return true;
    }

    bool readOccupancy(JSContext *ctx, int argc, JSValueConst *argv, int index, Occupancy &out) {
        out = Occupancy::Any;
        if (argc <= index || JS_IsUndefined(argv[index]))
            return true;
        if (!JS_IsString(argv[index]))
            return false;
        const std::string name = toString(ctx, argv[index]);
        if (name == "Any")
            out = Occupancy::Any;
        else if (name == "Full")
            out = Occupancy::Full;
        else if (name == "HasVacancy")
            out = Occupancy::HasVacancy;
        else
            return false;
        return true;
    }

    // a callback, then PoiNameFilter, then PoiTagFilter, as the variant lists them
    bool readFilter(JSContext *ctx, JSValueConst value, TypeFilter &out) {
        if (JS_IsFunction(ctx, value)) {
            out.mKind = TypeFilter::Kind::Callback;
            out.mCallback = value;
            return true;
        }
        if (!JS_IsObject(value))
            return false;
        JSValue name = JS_GetPropertyStr(ctx, value, "name");
        if (JS_IsString(name)) {
            out.mKind = TypeFilter::Kind::Name;
            out.mName = toString(ctx, name);
            JS_FreeValue(ctx, name);
            return true;
        }
        JS_FreeValue(ctx, name);
        JSValue tags = JS_GetPropertyStr(ctx, value, "tags");
        if (!JS_IsArray(tags)) {
            JS_FreeValue(ctx, tags);
            return false;
        }
        int64_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, tags, "length");
        JS_ToInt64(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        bool ok = true;
        for (int64_t index = 0; index < length && ok; ++index) {
            JSValue tag = JS_GetPropertyUint32(ctx, tags, (uint32_t) index);
            ok = JS_IsString(tag);
            if (ok)
                out.mTags.push_back(toString(ctx, tag));
            JS_FreeValue(ctx, tag);
        }
        JS_FreeValue(ctx, tags);
        out.mKind = TypeFilter::Kind::Tags;
        return ok;
    }

    // false with a pending exception when the callback threw
    bool filterMatches(ScriptApi &api, const TypeFilter &filter, uint16_t typeId, bool &matched) {
        matched = false;
        const PoiBlockType *type = PoiBlockTypes::get(typeId);
        if (type == nullptr)
            return true;
        switch (filter.mKind) {
            case TypeFilter::Kind::Name:
                matched = type->mName == filter.mName;
                return true;
            case TypeFilter::Kind::Tags:
                matched = std::all_of(filter.mTags.begin(), filter.mTags.end(),
                                      [&](const std::string &tag) { return type->hasTag(tag); });
                return true;
            case TypeFilter::Kind::Callback: {
                JSContext *ctx = api.context();
                JSValue argument = makeType(api, typeId);
                JSValue result = JS_Call(ctx, filter.mCallback, JS_UNDEFINED, 1, &argument);
                JS_FreeValue(ctx, argument);
                if (JS_IsException(result))
                    return false;
                matched = JS_ToBool(ctx, result) == 1;
                JS_FreeValue(ctx, result);
                return true;
            }
        }
        return true;
    }

    bool occupancyMatches(Occupancy occupancy, const PoiManager::Entry &entry) {
        switch (occupancy) {
            case Occupancy::Any:
                return true;
            // no ticket left, at least one left
            case Occupancy::Full:
                return entry.mTickets == 0;
            case Occupancy::HasVacancy:
                return entry.mTickets != 0;
        }
        return true;
    }

    int64_t distanceSquared(const Vector3i &a, const Vector3i &b) {
        const int64_t dx = (int64_t) a.x - b.x, dy = (int64_t) a.y - b.y, dz = (int64_t) a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    struct Query {
        ScriptApi *mApi = nullptr;
        Level *mLevel = nullptr;
        TypeFilter mFilter;
        Vector3i mCenter;
        uint16_t mRadius = 0;
        Occupancy mOccupancy = Occupancy::Any;
    };

    Level &levelOfManager(ScriptApi &api, JSValueConst thisVal) {
        const ScriptPoiDimension *dimension = api.native<ScriptPoiDimension>(thisVal);
        if (dimension == nullptr)
            return api.host().getLevel();
        return api.resolveLevel(dimension->mWorldId, dimension->mDimension);
    }

    // filter, center, blockRadius, then the optional occupancy filter at occupancyIndex
    bool readQuery(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int occupancyIndex,
                   Query &query) {
        query.mApi = apiOf(ctx);
        if (argc < 3 || !readFilter(ctx, argv[0], query.mFilter) || !readBlockPosition(ctx, argv[1], query.mCenter)
            || !readRadius(ctx, argv[2], query.mRadius))
            return false;
        if (occupancyIndex >= 0 && !readOccupancy(ctx, argc, argv, occupancyIndex, query.mOccupancy))
            return false;
        query.mLevel = &levelOfManager(*query.mApi, thisVal);
        return true;
    }

    // the box of blockRadius around the center, then the
    // distance check of getInRange when inRange is set; false when a callback threw
    bool collect(Query &query, bool inRange, const std::function<bool(PoiManager::Entry &)> &visit) {
        const Vector3i &center = query.mCenter;
        const int32_t radius = query.mRadius;
        const Vector3i min(center.x - radius, center.y - radius, center.z - radius);
        const Vector3i max(center.x + radius, center.y + radius, center.z + radius);
        const int64_t radiusSquared = (int64_t) radius * radius;
        // copies first: a filter callback may add or claim pois while the walk runs
        std::vector<PoiManager::Entry> candidates;
        query.mLevel->getPoiManager().forEachInBox(*query.mLevel, min, max, [&](PoiManager::Entry &entry) {
            if ((!inRange || distanceSquared(entry.mPosition, center) <= radiusSquared)
                && occupancyMatches(query.mOccupancy, entry))
                candidates.push_back(entry);
            return true;
        });
        for (PoiManager::Entry &candidate: candidates) {
            bool matched = false;
            if (!filterMatches(*query.mApi, query.mFilter, candidate.mType, matched))
                return false;
            if (matched && !visit(candidate))
                break;
        }
        return true;
    }

    JSValue managerGetBlocks(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = apiOf(ctx);
        const ScriptPoiDimension *dimension = api->native<ScriptPoiDimension>(thisVal);
        ScriptPoiDimension *blocks = new ScriptPoiDimension();
        if (dimension != nullptr) {
            blocks->mWorldId = dimension->mWorldId;
            blocks->mDimension = dimension->mDimension;
        }
        return api->makeInstance("PoiBlockManager", blocks);
    }

    JSValue dimensionGetPoiManager(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = apiOf(ctx);
        const Level &level = api->levelOf(thisVal);
        ScriptPoiDimension *manager = new ScriptPoiDimension();
        manager->mWorldId = level.getWorldId();
        manager->mDimension = level.getDimensionType();
        return api->makeInstance("PoiManager", manager);
    }

    JSValue blocksList(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, bool inRange) {
        Query query;
        if (!readQuery(ctx, thisVal, argc, argv, 3, query))
            return typeError(ctx);
        std::vector<PoiManager::Entry> found;
        if (!collect(query, inRange, [&](PoiManager::Entry &entry) {
                found.push_back(entry);
                return true;
            }))
            return JS_EXCEPTION;
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const PoiManager::Entry &entry: found)
            JS_SetPropertyUint32(ctx, array, index++, makePoiInstance(*query.mApi, entry));
        return array;
    }

    JSValue blocksGetInSquare(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return blocksList(ctx, thisVal, argc, argv, false);
    }

    JSValue blocksGetInRange(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return blocksList(ctx, thisVal, argc, argv, true);
    }

    JSValue blocksGetInRangeSorted(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        Query query;
        if (!readQuery(ctx, thisVal, argc, argv, 3, query))
            return typeError(ctx);
        std::vector<std::pair<int64_t, PoiManager::Entry>> found;
        if (!collect(query, true, [&](PoiManager::Entry &entry) {
                found.emplace_back(distanceSquared(entry.mPosition, query.mCenter), entry);
                return true;
            }))
            return JS_EXCEPTION;
        std::stable_sort(found.begin(), found.end(),
                         [](const std::pair<int64_t, PoiManager::Entry> &a,
                            const std::pair<int64_t, PoiManager::Entry> &b) { return a.first < b.first; });
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const auto &pair: found) {
            JSValue entry = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, entry, "distance", JS_NewFloat64(ctx, (double) pair.first));
            JS_SetPropertyStr(ctx, entry, "poi", makePoiInstance(*query.mApi, pair.second));
            JS_SetPropertyUint32(ctx, array, index++, entry);
        }
        return array;
    }

    // the first matching instance of getInRange with a ticket left gives one up
    JSValue blocksTake(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        Query query;
        if (!readQuery(ctx, thisVal, argc, argv, -1, query))
            return typeError(ctx);
        query.mOccupancy = Occupancy::HasVacancy;
        bool taken = false;
        Vector3i position;
        PoiManager &pois = query.mLevel->getPoiManager();
        if (!collect(query, true, [&](PoiManager::Entry &candidate) {
                PoiManager::Entry *entry = pois.at(*query.mLevel, candidate.mPosition);
                if (entry == nullptr || entry->mTickets == 0)
                    return true;
                --entry->mTickets;
                position = entry->mPosition;
                taken = true;
                return false;
            }))
            return JS_EXCEPTION;
        return taken ? blockPosition(ctx, position) : JS_UNDEFINED;
    }

    JSValue blocksRelease(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = apiOf(ctx);
        Vector3i position;
        if (argc < 1 || !readBlockPosition(ctx, argv[0], position))
            return typeError(ctx);
        Level &level = levelOfManager(*api, thisVal);
        return JS_NewBool(ctx, level.getPoiManager().release(level, position));
    }

    JSValue blocksAt(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = apiOf(ctx);
        Vector3i position;
        if (argc < 1 || !readBlockPosition(ctx, argv[0], position))
            return typeError(ctx);
        Level &level = levelOfManager(*api, thisVal);
        const PoiManager::Entry *entry = level.getPoiManager().at(level, position);
        if (entry == nullptr || PoiBlockTypes::get(entry->mType) == nullptr)
            return JS_UNDEFINED;
        return makeType(*api, entry->mType);
    }

    JSValue blocksExists(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = apiOf(ctx);
        Vector3i position;
        TypeFilter filter;
        if (argc < 2 || !readBlockPosition(ctx, argv[0], position) || !readFilter(ctx, argv[1], filter))
            return typeError(ctx);
        Level &level = levelOfManager(*api, thisVal);
        const PoiManager::Entry *entry = level.getPoiManager().at(level, position);
        if (entry == nullptr)
            return JS_FALSE;
        bool matched = false;
        if (!filterMatches(*api, filter, entry->mType, matched))
            return JS_EXCEPTION;
        return JS_NewBool(ctx, matched);
    }

    // a number is the id as it is, a name the registry's id or none, a type its own id
    JSValue blocksAddTemporary(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = apiOf(ctx);
        Vector3i position;
        if (argc < 2 || !readBlockPosition(ctx, argv[0], position))
            return typeError(ctx);
        uint16_t id = PoiBlockTypes::INVALID_ID;
        if (JS_IsNumber(argv[1])) {
            int32_t number = 0;
            JS_ToInt32(ctx, &number, argv[1]);
            id = (uint16_t) number;
        } else if (JS_IsString(argv[1])) {
            const PoiBlockType *type = PoiBlockTypes::find(toString(ctx, argv[1]));
            if (type != nullptr)
                id = type->mId;
        } else if (const ScriptPoiType *type = api->native<ScriptPoiType>(argv[1])) {
            id = type->mId;
        } else {
            return typeError(ctx);
        }
        Level &level = levelOfManager(*api, thisVal);
        level.getPoiManager().addTemporary(level, position, id);
        return JS_UNDEFINED;
    }

    enum class TypeField { Id, Name, Tickets, UsableRange };

    JSValue typeGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ScriptPoiType *ref = apiOf(ctx)->native<ScriptPoiType>(thisVal);
        const PoiBlockType *type = ref == nullptr ? nullptr : PoiBlockTypes::get(ref->mId);
        if (type == nullptr)
            return JS_UNDEFINED;
        switch ((TypeField) magic) {
            case TypeField::Id:
                return JS_NewInt32(ctx, type->mId);
            case TypeField::Name:
                return string(ctx, type->mName);
            case TypeField::Tickets:
                return JS_NewInt32(ctx, type->mTickets);
            case TypeField::UsableRange:
                return JS_NewInt32(ctx, type->mUsableRange);
        }
        return JS_UNDEFINED;
    }

    JSValue typeEquals(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = apiOf(ctx);
        const ScriptPoiType *self = api->native<ScriptPoiType>(thisVal);
        const ScriptPoiType *other = argc < 1 ? nullptr : api->native<ScriptPoiType>(argv[0]);
        if (other == nullptr)
            return typeError(ctx);
        return JS_NewBool(ctx, self != nullptr && self->mId == other->mId);
    }

    JSValue typeHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptPoiType *ref = apiOf(ctx)->native<ScriptPoiType>(thisVal);
        if (argc < 1 || !JS_IsString(argv[0]))
            return typeError(ctx);
        const PoiBlockType *type = ref == nullptr ? nullptr : PoiBlockTypes::get(ref->mId);
        return JS_NewBool(ctx, type != nullptr && type->hasTag(toString(ctx, argv[0])));
    }

    enum class InstanceField { Position, Tickets, Type };

    JSValue instanceGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi *api = apiOf(ctx);
        const ScriptPoiInstance *instance = api->native<ScriptPoiInstance>(thisVal);
        if (instance == nullptr)
            return JS_UNDEFINED;
        switch ((InstanceField) magic) {
            case InstanceField::Position:
                return blockPosition(ctx, instance->mPosition);
            case InstanceField::Tickets:
                return JS_NewInt32(ctx, instance->mTickets);
            case InstanceField::Type:
                // an instance added under an unknown name points at no record
                return PoiBlockTypes::get(instance->mType) == nullptr ? JS_UNDEFINED : makeType(*api, instance->mType);
        }
        return JS_UNDEFINED;
    }
}

void ScriptApi::_bindPoi() {
    // the classes only exist under the experiment, as the game registers them
    if (!mSurface.has("PoiManager") || !mHost.getLevel().hasExperiment(POI_EXPERIMENT))
        return;

    property(prototypeOf("Dimension"), "Dimension", "poiManager", dimensionGetPoiManager);

    JSValue manager = defineClass("PoiManager");
    property(manager, "PoiManager", "blocks", managerGetBlocks);

    JSValue blocks = defineClass("PoiBlockManager");
    method(blocks, "PoiBlockManager", "addTemporary", blocksAddTemporary, 2);
    method(blocks, "PoiBlockManager", "at", blocksAt, 1);
    method(blocks, "PoiBlockManager", "exists", blocksExists, 2);
    method(blocks, "PoiBlockManager", "getInRange", blocksGetInRange, 4);
    method(blocks, "PoiBlockManager", "getInRangeSorted", blocksGetInRangeSorted, 4);
    method(blocks, "PoiBlockManager", "getInSquare", blocksGetInSquare, 4);
    method(blocks, "PoiBlockManager", "release", blocksRelease, 1);
    method(blocks, "PoiBlockManager", "take", blocksTake, 3);

    JSValue type = defineClass("PoiBlockType");
    propertyMagic(type, "PoiBlockType", "id", typeGet, (int) TypeField::Id);
    propertyMagic(type, "PoiBlockType", "name", typeGet, (int) TypeField::Name);
    propertyMagic(type, "PoiBlockType", "tickets", typeGet, (int) TypeField::Tickets);
    propertyMagic(type, "PoiBlockType", "usableRange", typeGet, (int) TypeField::UsableRange);
    method(type, "PoiBlockType", "equals", typeEquals, 1);
    method(type, "PoiBlockType", "hasTag", typeHasTag, 1);

    JSValue instance = defineClass("PoiBlockInstance");
    propertyMagic(instance, "PoiBlockInstance", "position", instanceGet, (int) InstanceField::Position);
    propertyMagic(instance, "PoiBlockInstance", "tickets", instanceGet, (int) InstanceField::Tickets);
    propertyMagic(instance, "PoiBlockInstance", "type", instanceGet, (int) InstanceField::Type);
}
