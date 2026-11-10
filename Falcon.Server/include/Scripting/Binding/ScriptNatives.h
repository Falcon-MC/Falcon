#pragma once

#include "World/Actor/DynamicPropertyStore.h"
#include "World/Level/Dimension/Dimension.h"
#include "Core/NBT/Tag.h"

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

// the stack's own NBT (name, lore, enchantments, lock, book, color) stays in mTag as the game saves it
struct ScriptItemData {
    std::string mTypeId;
    int32_t mAmount = 1;
    int32_t mDamage = 0;
    Tag mTag = Tag::ofCompound();
    std::vector<std::string> mCanPlaceOn;
    std::vector<std::string> mCanDestroy;
    std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
};

// item components share the stack: writes through the component show on the ItemStack
struct ScriptItemRef : ScriptNative {
    std::shared_ptr<ScriptItemData> mItem;
    // set on an item component, empty on the ItemStack itself
    std::string mComponentId;
};

// an entity's container, one of its slots, or the storage of an item; read on every access
struct ScriptContainerRef : ScriptEntityPart {
    enum class Kind { Inventory, Ender, Equipment, Cursor, Item, Block };

    Kind mKind = Kind::Inventory;
    // Kind::Item: the bundle the container lives in
    std::shared_ptr<ScriptItemData> mItem;
    // a ContainerSlot: its index, or its EquipmentSlot name; -1 and empty on the Container
    int32_t mSlot = -1;
    std::string mEquipmentSlot;
    // the Container itself, not its owning component
    bool mIsContainer = false;
    // Kind::Block: the block actor holding the container
    uint32_t mWorldId = 0;
    DimensionType mDimension = DimensionType::Overworld;
    int32_t mX = 0;
    int32_t mY = 0;
    int32_t mZ = 0;
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

struct ScriptEffectRef : ScriptNative {
    ScriptEntityRef mEntity;
    int32_t mEffectId = 0;
};

// EffectType, EntityType and EntityHealSource: one name each
struct ScriptNamedRef : ScriptNative {
    std::string mName;
};

struct ScriptInputInfoRef : ScriptNative {
    uint32_t mHandle = 0;
};

struct ScriptSignalRef : ScriptNative {
    std::string mKey;
};

struct ScriptRegistryRef : ScriptNative {
    bool mBlocks = true;
};

// a world clock, or one of its time markers when mMarker is set; both read the clock system on access
struct ScriptClockRef : ScriptNative {
    std::string mClock;
    std::string mMarker;
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

    // setDynamicProperties: false with a pending exception
    bool dynamicPropertySetAll(JSContext *ctx, std::unordered_map<std::string, DynamicPropertyValue> &store,
                               JSValueConst values);

    int64_t dynamicPropertyByteCount(const std::unordered_map<std::string, DynamicPropertyValue> &store);

    bool arrayContainsString(JSContext *ctx, JSValueConst array, const std::string &needle);

    // block states: bytes are booleans, as the palette stores them
    JSValue statesToJs(JSContext *ctx, const Tag &states);

    Tag statesFromJs(JSContext *ctx, JSValueConst object);
}
