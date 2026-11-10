#pragma once

#include "World/Actor/DynamicPropertyStore.h"
#include "World/Level/Dimension/Dimension.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <quickjs.h>

// opaque of every object the scripting layer hands out: scripts only see the official members
struct ScriptNative {
    virtual ~ScriptNative() = default;
};

struct ScriptEntityRef : ScriptNative {
    enum class Kind { Player, Actor, Item };

    Kind mKind = Kind::Actor;
    // player handle, actor unique id or item entity runtime id
    int64_t mId = 0;
    // kept for InvalidEntityError once the entity is gone
    std::string mEntityId;
    std::string mTypeId;
};

// a part of one player: screen display, camera, inventory, equipment slot
struct ScriptPlayerPart : ScriptNative {
    uint32_t mHandle = 0;
    std::string mSlot;
    std::string mComponentId;
};

// an entity component that reads its entity on every access
struct ScriptEntityPart : ScriptNative {
    ScriptEntityRef mEntity;
    std::string mComponentId;
};

struct ScriptItemData {
    std::string mTypeId;
    int32_t mAmount = 1;
    int32_t mDamage = 0;
    std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
};

// item components share the stack: writes through the component show on the ItemStack
struct ScriptItemRef : ScriptNative {
    std::shared_ptr<ScriptItemData> mItem;
    // set on an item component, empty on the ItemStack itself
    std::string mComponentId;
};

struct ScriptBlockRef : ScriptNative {
    DimensionType mDimension = DimensionType::Overworld;
    uint32_t mWorldId = 0;
    int32_t mX = 0;
    int32_t mY = 0;
    int32_t mZ = 0;
};

struct ScriptDimensionRef : ScriptNative {
    DimensionType mDimension = DimensionType::Overworld;
    uint32_t mWorldId = 0;
};

struct ScriptSignalRef : ScriptNative {
    std::string mKey;
};

struct ScriptRegistryRef : ScriptNative {
    bool mBlocks = true;
};

namespace ScriptValues {
    std::string toString(JSContext *ctx, JSValueConst value);

    JSValue string(JSContext *ctx, const std::string &value);

    JSValue vector3(JSContext *ctx, float x, float y, float z);

    bool readVector3(JSContext *ctx, JSValueConst value, double &x, double &y, double &z);

    std::string messageToRawText(JSContext *ctx, JSValueConst value, bool &isJson);

    JSValue dynamicPropertyGet(JSContext *ctx, const std::unordered_map<std::string, DynamicPropertyValue> &store,
                               const std::string &id);

    void dynamicPropertySet(JSContext *ctx, std::unordered_map<std::string, DynamicPropertyValue> &store,
                            const std::string &id, JSValueConst value);

    JSValue dynamicPropertyIds(JSContext *ctx, const std::unordered_map<std::string, DynamicPropertyValue> &store);

    bool arrayContainsString(JSContext *ctx, JSValueConst array, const std::string &needle);
}
