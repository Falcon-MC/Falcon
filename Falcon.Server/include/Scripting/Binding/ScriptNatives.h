#pragma once

#include "World/Actor/DynamicPropertyStore.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/NeighborChangeQueue.h"
#include "Core/NBT/Tag.h"
#include "World/Scores/Scoreboard.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <quickjs.h>

// live handles of one pack by class name, as collectPluginStats reports them
using ScriptHandleCounts = std::unordered_map<std::string, int64_t>;

// opaque of every object the scripting layer hands out: scripts only see the official members
struct ScriptNative {
    virtual ~ScriptNative() {
        if (mHandleCount != nullptr)
            --*mHandleCount;
    }

    // the counts outlive the pack's api: objects are finalized with the runtime
    std::shared_ptr<ScriptHandleCounts> mHandleCounts;
    int64_t *mHandleCount = nullptr;
};

struct ScriptEntityRef : ScriptNative {
    enum class Kind { Player, Actor, Item };

    Kind mKind = Kind::Actor;
    // player handle or runtime id: a unique id can still change once the object exists
    int64_t mId = 0;
    // refreshed on every resolve, kept for InvalidEntityError once the entity is gone
    mutable std::string mEntityId;
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

// one player's stonecutter window, valid while that window stays open
struct ScriptRecipeCraftingContext : ScriptNative {
    uint32_t mHandle = 0;
    int mWindowId = -1;
    ScriptBlockRef mBlock;
};

// a block component: reads the block on every access
struct ScriptBlockPart : ScriptNative {
    ScriptBlockRef mBlock;
    std::string mComponentId;
};

// a BlockComponentNamedTickEvent: the name and namespace isName compares
struct ScriptNamedTickEvent : ScriptNative {
    std::string mNamespace;
    std::string mEventName;
};

// a BlockComponentNeighborChangedAfterEvent: the changes getChanges hands out
struct ScriptNeighborChangedEvent : ScriptNative {
    std::vector<NeighborChangeQueue::Change> mChanges;
};

// one entry of BlockEntityStorageComponent.getEntities: a copy, the stored entity can leave afterwards
struct ScriptEntityStorageInfo : ScriptNative {
    std::string mUniqueId;
    std::string mDefinitionId;
};

// a structure of the shared registry, by identifier: deleting it leaves the object invalid
struct ScriptStructureRef : ScriptNative {
    std::string mId;
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

// an objective by name and serial: a removed objective stays invalid once another takes its name
struct ScriptObjectiveRef : ScriptNative {
    std::string mName;
    uint64_t mSerial = 0;
};

// a ScoreboardIdentity, or a ScoreboardScoreInfo with its score; the identity is kept for an invalid one
struct ScriptIdentityRef : ScriptNative {
    ScoreboardIdentity mIdentity;
    int32_t mScore = 0;
};

// one WaypointTextureBounds: a WaypointTexture value or a CustomTexture path with its icon size
struct ScriptWaypointTexture {
    double mLowerBound = 0.0;
    bool mHasUpperBound = false;
    double mUpperBound = 0.0;
    bool mCustom = false;
    std::string mTexture;
    double mIconWidth = 0.0;
    double mIconHeight = 0.0;
};

// shared by the waypoint object and every locator bar holding it
struct ScriptWaypointState {
    enum class Kind { Location, Entity, Player };

    Kind mKind = Kind::Location;
    ScriptEntityRef mEntity;
    DimensionType mDimension = DimensionType::Overworld;
    uint32_t mWorldId = 0;
    double mX = 0.0, mY = 0.0, mZ = 0.0;
    std::vector<ScriptWaypointTexture> mTextures;
    bool mHasColor = false;
    double mRed = 0.0, mGreen = 0.0, mBlue = 0.0;
    bool mEnabled = true;
    bool mRemoved = false;
    bool mShowDead = true, mShowInvisible = true, mShowSneaking = true;
    bool mShowHidden = true, mShowSpectator = true, mShowSpectatorToSpectator = true;
};

struct ScriptWaypointRef : ScriptNative {
    std::shared_ptr<ScriptWaypointState> mState;
};

// a played sound: the controls go to its recipients under the server sound handle
struct ScriptSoundState {
    int64_t mHandle = 0;
    std::string mEventId;
    // player handles the sound was sent to; mRecipient only when Player.playSound played it
    std::vector<uint32_t> mListeners;
    uint32_t mRecipient = 0;
    bool mHasDuration = false;
    // the definition's units: milliseconds for real_time, ticks for game_time
    int64_t mDurationUnits = 0;
    bool mRealTime = false;
    // -1 loops until stopped or until scripts drop the last SoundInstance
    int32_t mLoopCount = 0;
    double mElapsedUnits = 0.0;
    std::chrono::steady_clock::time_point mLastRealTime;
    bool mPaused = false;
    bool mStopped = false;
    bool mCompleted = false;

    double unitsPerSecond() const {
        return mRealTime ? 1000.0 : 20.0;
    }

    float durationSeconds() const {
        return (float) mDurationUnits / (mRealTime ? 1000.0f : 20.0f);
    }
};

// SoundInstance and its SoundDurationInfo share the state
struct ScriptSoundRef : ScriptNative {
    std::shared_ptr<ScriptSoundState> mState;
};

struct ScriptSoundDefinitionRef : ScriptNative {
    std::string mEventId;
};

// a MolangVariableMap: a variable holds a float, or named member floats (".r", ".x", ".speed")
struct ScriptMolangMap : ScriptNative {
    struct Variable {
        std::string mName;
        float mValue = 0.0f;
        bool mArray = false;
        std::vector<std::pair<std::string, float>> mMembers;
    };

    std::vector<Variable> mVariables;

    std::string toJson() const;
};

// a player's FogSettings: the player it reads on every call
struct ScriptFogRef : ScriptNative {
    ScriptEntityRef mEntity;
};

// AimAssistCategorySettings: what addCategory reads; both defaults start at 1 (sub_14C936250, 26.60)
struct ScriptAimAssistCategorySettingsRef : ScriptNative {
    std::string mIdentifier;
    std::vector<std::pair<std::string, int32_t>> mEntityPriorities;
    std::vector<std::pair<std::string, int32_t>> mBlockPriorities;
    std::vector<std::pair<std::string, int32_t>> mBlockTagPriorities;
    std::vector<std::pair<std::string, int32_t>> mEntityTypeFamilyPriorities;
    int32_t mDefaultEntityPriority = 1;
    int32_t mDefaultBlockPriority = 1;
};

// AimAssistPresetSettings: unset lists read undefined
struct ScriptAimAssistPresetSettingsRef : ScriptNative {
    std::string mIdentifier;
    std::optional<std::vector<std::string>> mExcludedBlocks;
    std::optional<std::vector<std::string>> mExcludedEntities;
    std::optional<std::vector<std::string>> mExcludedBlockTags;
    std::optional<std::vector<std::string>> mExcludedEntityTypeFamilies;
    std::optional<std::vector<std::string>> mLiquidTargetingItems;
    std::vector<std::pair<std::string, std::string>> mItemSettings;
    std::optional<std::string> mDefaultItemSettings;
    std::optional<std::string> mHandSettings;
};

// an AimAssistPreset or AimAssistCategory: its identifier, read from the registry on every access
struct ScriptAimAssistRef : ScriptNative {
    std::string mIdentifier;
};

// a player's PlayerAimAssist
struct ScriptPlayerAimAssistRef : ScriptNative {
    ScriptEntityRef mEntity;
};

// a CatmullRomSpline or a LinearSpline: only its control points
struct ScriptSplineRef : ScriptNative {
    bool mLinear = false;
    std::vector<std::array<float, 3>> mPoints;
};

// a read-only record copied out of an entity definition: Seat, FeedItem, FeedItemEffect, EntityDefinitionFeedItem
struct ScriptRecordRef : ScriptNative {
    std::unordered_map<std::string, double> mNumbers;
    std::unordered_map<std::string, std::string> mStrings;
    float mVector[3] = {0.0f, 0.0f, 0.0f};
    std::vector<std::shared_ptr<ScriptRecordRef>> mChildren;
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

    // a bound as the binding prints it (sub_148552370): whole numbers bare, others with two decimals
    std::string boundText(double value);

    // block states: bytes are booleans, as the palette stores them
    JSValue statesToJs(JSContext *ctx, const Tag &states);

    Tag statesFromJs(JSContext *ctx, JSValueConst object);
}
