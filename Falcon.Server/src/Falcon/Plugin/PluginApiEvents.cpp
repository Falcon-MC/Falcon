#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginServerApi.h"
#include "Protocol/Types/ItemStack.h"

#include "Core/Json/Json.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace PluginApiHelpers;

namespace {
    FalconEventType eventType(FalconEvent *target) {
        return event(target)->mType;
    }

    int eventIsCancellable(FalconEvent *target) {
        return event(target)->mCancellable ? 1 : 0;
    }

    int eventIsCancelled(FalconEvent *target) {
        return event(target)->mCancelled ? 1 : 0;
    }

    void eventSetCancelled(FalconEvent *target, int cancelled) {
        PluginEvent *source = event(target);
        if (source->mMonitor || !source->mCancellable)
            return;
        source->mCancelled = cancelled != 0;
    }

    FalconPlayer *eventPlayer(FalconEvent *target) {
        return toHandle(event(target)->mPlayer);
    }

    const char *eventMessage(FalconEvent *target) {
        PluginEvent *source = event(target);
        if (source->mMessage == nullptr)
            return nullptr;
        return hold(*source->mMessage);
    }

    void eventSetMessage(FalconEvent *target, const char *message) {
        PluginEvent *source = event(target);
        if (source->mMonitor || source->mMessage == nullptr || message == nullptr)
            return;
        *source->mMessage = message;
        source->mMessageDefault = false;
    }

    int eventMessageIsDefault(FalconEvent *target) {
        return event(target)->mMessageDefault ? 1 : 0;
    }

    FalconEntity *eventEntity(FalconEvent *target) {
        return toHandle(event(target)->mEntity);
    }

    FalconEntity *eventAttacker(FalconEvent *target) {
        return toHandle(event(target)->mAttacker);
    }

    FalconBlockPos eventBlockPosition(FalconEvent *target) {
        const Vector3i &position = event(target)->mBlockPosition;
        return FalconBlockPos{position.x, position.y, position.z};
    }

    uint32_t eventBlockFace(FalconEvent *target) {
        return event(target)->mBlockFace;
    }

    const char *eventBlockName(FalconEvent *target) {
        return hold(event(target)->mBlockName);
    }

    double eventAmount(FalconEvent *target) {
        return event(target)->mAmount;
    }

    void eventSetAmount(FalconEvent *target, double amount) {
        PluginEvent *source = event(target);
        if (source->mMonitor)
            return;
        source->mAmount = amount;
    }

    const char *eventCause(FalconEvent *target) {
        return hold(event(target)->mCause);
    }

    FalconVec3 eventFrom(FalconEvent *target) {
        const Vector3f &position = event(target)->mFrom;
        return FalconVec3{position.x, position.y, position.z};
    }

    FalconVec3 eventTo(FalconEvent *target) {
        const Vector3f &position = event(target)->mTo;
        return FalconVec3{position.x, position.y, position.z};
    }

    void eventSetTo(FalconEvent *target, FalconVec3 position) {
        PluginEvent *source = event(target);
        if (source->mMonitor)
            return;
        source->mTo = Vector3f((float) position.x, (float) position.y, (float) position.z);
        source->mToChanged = true;
    }

    FalconItem *eventItem(FalconEvent *target) {
        return toHandle(event(target)->mItem);
    }

    uint32_t eventPacketId(FalconEvent *target) {
        return event(target)->mPacketId;
    }

    const uint8_t *eventPacketData(FalconEvent *target, uint32_t *length) {
        PluginEvent *source = event(target);
        if (source->mPacketData == nullptr) {
            if (length != nullptr)
                *length = 0;
            return nullptr;
        }
        if (length != nullptr)
            *length = (uint32_t) source->mPacketData->size();
        return reinterpret_cast<const uint8_t *>(source->mPacketData->data());
    }

    void eventSetPacketData(FalconEvent *target, const uint8_t *data, uint32_t length) {
        PluginEvent *source = event(target);
        if (source->mMonitor || source->mPacketData == nullptr)
            return;
        if (data == nullptr) {
            source->mPacketData->clear();
            return;
        }
        source->mPacketData->assign(reinterpret_cast<const char *>(data), length);
    }

    FalconLevel *eventLevel(FalconEvent *target) {
        return toHandle(event(target)->mLevel);
    }

    FalconEntity *eventTarget(FalconEvent *target) {
        return toHandle(event(target)->mTarget);
    }

    FalconVec3 eventPosition(FalconEvent *target) {
        const Vector3f &position = event(target)->mPosition;
        return FalconVec3{position.x, position.y, position.z};
    }

    uint32_t eventBlockCount(FalconEvent *target) {
        const PluginEvent *source = event(target);
        if (source->mBlocks == nullptr)
            return 0;
        return (uint32_t) source->mBlocks->size();
    }

    FalconBlockPos eventBlockAt(FalconEvent *target, uint32_t index) {
        const PluginEvent *source = event(target);
        if (source->mBlocks == nullptr || index >= source->mBlocks->size())
            return FalconBlockPos{0, 0, 0};

        const Vector3i &position = (*source->mBlocks)[index];
        return FalconBlockPos{position.x, position.y, position.z};
    }

    void eventSetBlocks(FalconEvent *target, const FalconBlockPos *positions, uint32_t count) {
        PluginEvent *source = event(target);
        if (source->mMonitor || source->mBlocks == nullptr)
            return;

        source->mBlocks->clear();
        if (positions == nullptr)
            return;

        source->mBlocks->reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            source->mBlocks->emplace_back(positions[index].x, positions[index].y, positions[index].z);
        }
    }

    FalconGameMode eventGameMode(FalconEvent *target) {
        return (FalconGameMode) event(target)->mGameMode;
    }

    FalconGameMode eventPreviousGameMode(FalconEvent *target) {
        return (FalconGameMode) event(target)->mPreviousGameMode;
    }

    FalconDimension eventDimension(FalconEvent *target) {
        return event(target)->mDimension;
    }

    FalconDimension eventPreviousDimension(FalconEvent *target) {
        return event(target)->mPreviousDimension;
    }

    uint64_t eventTick(FalconEvent *target) {
        return event(target)->mTick;
    }

    const char *eventSourceContainer(FalconEvent *target) {
        return hold(event(target)->mSourceContainer);
    }

    int32_t eventSourceSlot(FalconEvent *target) {
        return event(target)->mSourceSlot;
    }

    const char *eventDestinationContainer(FalconEvent *target) {
        return hold(event(target)->mDestinationContainer);
    }

    int32_t eventDestinationSlot(FalconEvent *target) {
        return event(target)->mDestinationSlot;
    }

    int eventState(FalconEvent *target) {
        return event(target)->mState ? 1 : 0;
    }

    double eventPreviousAmount(FalconEvent *target) {
        return event(target)->mPreviousAmount;
    }

    const char *eventPreviousBlockName(FalconEvent *target) {
        return hold(event(target)->mPreviousBlockName);
    }

    int32_t eventChunkX(FalconEvent *target) {
        return event(target)->mChunkX;
    }

    int32_t eventChunkZ(FalconEvent *target) {
        return event(target)->mChunkZ;
    }

    FalconItem *eventResult(FalconEvent *target) {
        return toHandle(event(target)->mResult);
    }

    void eventSetResult(FalconEvent *target, FalconItem *result) {
        PluginEvent *source = event(target);
        if (source->mMonitor || source->mResult == nullptr || result == nullptr)
            return;
        *source->mResult = *item(result);
    }

    const char *eventWorldName(FalconEvent *target) {
        return hold(event(target)->mWorldName);
    }

    const char *eventPreviousWorldName(FalconEvent *target) {
        return hold(event(target)->mPreviousWorldName);
    }

    const char *eventAddress(FalconEvent *target) {
        return hold(event(target)->mAddress);
    }

    const char *eventTransport(FalconEvent *target) {
        return hold(event(target)->mTransport);
    }

    uint64_t eventClientGuid(FalconEvent *target) {
        return event(target)->mClientGuid;
    }

    uint32_t eventMtuSize(FalconEvent *target) {
        return event(target)->mMtuSize;
    }

    const char *eventClientData(FalconEvent *target) {
        const std::string *clientData = event(target)->mClientData;
        return hold(clientData != nullptr ? *clientData : std::string());
    }

    uint32_t eventFormId(FalconEvent *target) {
        return event(target)->mFormId;
    }

    int eventEffect(FalconEvent *target, FalconEffect *out) {
        const PluginEvent *source = event(target);
        if (!source->mHasEffect || out == nullptr)
            return 0;
        *out = source->mEffect;
        return 1;
    }

    const char *eventFormResponse(FalconEvent *target) {
        return hold(event(target)->mFormResponse);
    }

    const char *eventClientDataField(FalconEvent *target, const char *key) {
        const std::string *clientData = event(target)->mClientData;
        if (clientData == nullptr || key == nullptr)
            return hold(std::string());

        const std::unique_ptr<json::Value> root = json::parse(*clientData);
        const json::Value *value = root != nullptr ? root->get(key) : nullptr;
        if (value == nullptr)
            return hold(std::string());

        switch (value->mType) {
            case json::Value::Type::String:
                return hold(value->mString);
            case json::Value::Type::Boolean:
                return hold(value->mBoolean ? "true" : "false");
            case json::Value::Type::Number: {
                const double number = value->mNumber;
                if (std::floor(number) == number && std::fabs(number) < 9.0e15)
                    return hold(std::to_string((long long) number));
                return hold(std::to_string(number));
            }
            default:
                return hold(std::string());
        }
    }
}

namespace {
    uint32_t eventDropCount(FalconEvent *target) {
        const PluginEvent *source = event(target);
        return source->mDrops == nullptr ? 0 : (uint32_t) source->mDrops->size();
    }

    FalconItem *eventDropAt(FalconEvent *target, uint32_t index) {
        PluginEvent *source = event(target);
        if (source->mDrops == nullptr || index >= source->mDrops->size())
            return nullptr;
        return toHandle(&(*source->mDrops)[index]);
    }

    void eventSetDrops(FalconEvent *target, FalconItem *const *items, uint32_t count) {
        PluginEvent *source = event(target);
        if (source->mMonitor || source->mDrops == nullptr || (items == nullptr && count > 0))
            return;
        std::vector<ItemStack> drops;
        drops.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            if (items[index] != nullptr && !item(items[index])->isAir())
                drops.push_back(*item(items[index]));
        }
        *source->mDrops = std::move(drops);
    }

    int32_t eventExperience(FalconEvent *target) {
        const PluginEvent *source = event(target);
        return source->mExperience == nullptr ? 0 : *source->mExperience;
    }

    void eventSetExperience(FalconEvent *target, int32_t experience) {
        PluginEvent *source = event(target);
        if (!source->mMonitor && source->mExperience != nullptr)
            *source->mExperience = std::max(0, experience);
    }

    int eventKeepInventory(FalconEvent *target) {
        const PluginEvent *source = event(target);
        return source->mKeepInventory != nullptr && *source->mKeepInventory ? 1 : 0;
    }

    void eventSetKeepInventory(FalconEvent *target, int keepInventory) {
        PluginEvent *source = event(target);
        if (!source->mMonitor && source->mKeepInventory != nullptr)
            *source->mKeepInventory = keepInventory != 0;
    }

    int eventKeepExperience(FalconEvent *target) {
        const PluginEvent *source = event(target);
        return source->mKeepExperience != nullptr && *source->mKeepExperience ? 1 : 0;
    }

    void eventSetKeepExperience(FalconEvent *target, int keepExperience) {
        PluginEvent *source = event(target);
        if (!source->mMonitor && source->mKeepExperience != nullptr)
            *source->mKeepExperience = keepExperience != 0;
    }

    int eventKnockback(FalconEvent *target, float *horizontal, float *vertical) {
        const PluginEvent *source = event(target);
        if (source->mKnockbackHorizontal == nullptr || source->mKnockbackVertical == nullptr)
            return 0;
        if (horizontal != nullptr)
            *horizontal = *source->mKnockbackHorizontal;
        if (vertical != nullptr)
            *vertical = *source->mKnockbackVertical;
        return 1;
    }

    void eventSetKnockback(FalconEvent *target, float horizontal, float vertical) {
        PluginEvent *source = event(target);
        if (source->mMonitor || source->mKnockbackHorizontal == nullptr || source->mKnockbackVertical == nullptr)
            return;
        if (!std::isfinite(horizontal) || !std::isfinite(vertical))
            return;
        *source->mKnockbackHorizontal = std::max(0.0f, horizontal);
        *source->mKnockbackVertical = std::max(0.0f, vertical);
    }
}

void PluginServerApi::fillEvents(FalconServerApi &api) {
    api.eventType = &eventType;
    api.eventIsCancellable = &eventIsCancellable;
    api.eventIsCancelled = &eventIsCancelled;
    api.eventSetCancelled = &eventSetCancelled;
    api.eventPlayer = &eventPlayer;
    api.eventMessage = &eventMessage;
    api.eventSetMessage = &eventSetMessage;
    api.eventMessageIsDefault = &eventMessageIsDefault;
    api.eventEntity = &eventEntity;
    api.eventAttacker = &eventAttacker;
    api.eventBlockPosition = &eventBlockPosition;
    api.eventBlockFace = &eventBlockFace;
    api.eventBlockName = &eventBlockName;
    api.eventAmount = &eventAmount;
    api.eventSetAmount = &eventSetAmount;
    api.eventCause = &eventCause;
    api.eventFrom = &eventFrom;
    api.eventTo = &eventTo;
    api.eventSetTo = &eventSetTo;
    api.eventItem = &eventItem;
    api.eventPacketId = &eventPacketId;
    api.eventPacketData = &eventPacketData;
    api.eventSetPacketData = &eventSetPacketData;
    api.eventLevel = &eventLevel;
    api.eventTarget = &eventTarget;
    api.eventPosition = &eventPosition;
    api.eventBlockCount = &eventBlockCount;
    api.eventBlockAt = &eventBlockAt;
    api.eventSetBlocks = &eventSetBlocks;
    api.eventGameMode = &eventGameMode;
    api.eventPreviousGameMode = &eventPreviousGameMode;
    api.eventDimension = &eventDimension;
    api.eventPreviousDimension = &eventPreviousDimension;
    api.eventTick = &eventTick;
    api.eventSourceContainer = &eventSourceContainer;
    api.eventSourceSlot = &eventSourceSlot;
    api.eventDestinationContainer = &eventDestinationContainer;
    api.eventDestinationSlot = &eventDestinationSlot;
    api.eventState = &eventState;
    api.eventPreviousAmount = &eventPreviousAmount;
    api.eventPreviousBlockName = &eventPreviousBlockName;
    api.eventChunkX = &eventChunkX;
    api.eventChunkZ = &eventChunkZ;
    api.eventResult = &eventResult;
    api.eventSetResult = &eventSetResult;
    api.eventWorldName = &eventWorldName;
    api.eventPreviousWorldName = &eventPreviousWorldName;
    api.eventAddress = &eventAddress;
    api.eventTransport = &eventTransport;
    api.eventClientGuid = &eventClientGuid;
    api.eventMtuSize = &eventMtuSize;
    api.eventClientData = &eventClientData;
    api.eventClientDataField = &eventClientDataField;
    api.eventFormId = &eventFormId;
    api.eventFormResponse = &eventFormResponse;
    api.eventEffect = &eventEffect;
    api.eventDropCount = &eventDropCount;
    api.eventDropAt = &eventDropAt;
    api.eventSetDrops = &eventSetDrops;
    api.eventExperience = &eventExperience;
    api.eventSetExperience = &eventSetExperience;
    api.eventKeepInventory = &eventKeepInventory;
    api.eventSetKeepInventory = &eventSetKeepInventory;
    api.eventKeepExperience = &eventKeepExperience;
    api.eventSetKeepExperience = &eventSetKeepExperience;
    api.eventKnockback = &eventKnockback;
    api.eventSetKnockback = &eventSetKnockback;
}
