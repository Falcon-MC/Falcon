#include "Protocol/InventoryCodec.h"

#include "Core/Utility/BinaryDataException.h"
#include "Protocol/ItemCodec.h"

namespace {

    void skipRecipeIngredient(ReadOnlyBinaryStream &stream) {
        const uint32_t descriptorType = stream.getUnsignedVarInt();
        stream.getByte();

        if (descriptorType == 1) {
            stream.getString();
            stream.getVarInt();
        } else if (descriptorType == 2) {
            stream.getString();
            stream.getLShort();
        } else if (descriptorType == 3) {
            stream.getString();
        }

        stream.getLShort();
    }

    int32_t itemStackRequestActionJavaOrdinal(ItemStackRequestActionType type) {
        switch (type) {
            case ItemStackRequestActionType::Take:
                return 0;
            case ItemStackRequestActionType::Place:
                return 1;
            case ItemStackRequestActionType::Swap:
                return 2;
            case ItemStackRequestActionType::Drop:
                return 3;
            case ItemStackRequestActionType::Destroy:
                return 4;
            case ItemStackRequestActionType::Consume:
                return 5;
            case ItemStackRequestActionType::Create:
                return 6;
            case ItemStackRequestActionType::LabTableCombine:
                return 9;
            case ItemStackRequestActionType::BeaconPayment:
                return 10;
            case ItemStackRequestActionType::MineBlock:
                return 11;
            case ItemStackRequestActionType::CraftRecipe:
                return 12;
            case ItemStackRequestActionType::CraftRecipeAuto:
                return 13;
            case ItemStackRequestActionType::CraftCreative:
                return 14;
            case ItemStackRequestActionType::CraftRecipeOptional:
                return 15;
            case ItemStackRequestActionType::CraftRepairAndDisenchant:
                return 16;
            case ItemStackRequestActionType::CraftLoom:
                return 17;
            case ItemStackRequestActionType::CraftNonImplemented:
                return 18;
            case ItemStackRequestActionType::CraftResultsDeprecated:
                return 19;
        }
        return 0;
    }

}

void InventoryCodec::writeFullContainerName(BinaryStream &stream, const FullContainerName &name) {
    stream.putByte((unsigned char) containerSlotTypeToId(name.mContainer));
    stream.putOptionalPresent(name.mHasDynamicId);
    if (name.mHasDynamicId) {
        stream.putLInt((uint32_t) name.mDynamicId);
    }
}

FullContainerName InventoryCodec::readFullContainerName(ReadOnlyBinaryStream &stream) {
    FullContainerName name;
    name.mContainer = containerSlotTypeFromId((int32_t) stream.getByte());
    name.mHasDynamicId = stream.getOptionalPresent();
    if (name.mHasDynamicId) {
        name.mDynamicId = (int32_t) stream.getLInt();
    }
    return name;
}

void InventoryCodec::writeInventorySource(BinaryStream &stream, const InventorySource &source) {
    stream.putUnsignedVarInt((uint32_t) inventorySourceTypeToId(source.mType));

    switch (source.mType) {
        case InventorySourceType::Container:
        case InventorySourceType::NonImplementedTodo:
            stream.putBool(true);
            stream.putByte((unsigned char) source.mContainerId);
            break;
        default:
            stream.putBool(false);
            break;
    }

    if (source.mType == InventorySourceType::WorldInteraction) {
        stream.putBool(true);
        stream.putUnsignedVarInt((uint32_t) source.mFlag);
    } else {
        stream.putBool(false);
    }
}

InventorySource InventoryCodec::readInventorySource(ReadOnlyBinaryStream &stream) {
    InventorySource source;
    source.mType = inventorySourceTypeFromId((int32_t) stream.getUnsignedVarInt());

    if (stream.getBool()) {
        source.mContainerId = stream.getByte();
    }

    if (stream.getBool()) {
        source.mFlag = (InventorySourceFlag) stream.getUnsignedVarInt();
    }

    return source;
}

void InventoryCodec::writeInventoryActions(BinaryStream &stream, const PacketCodecContext &context,
                                            const std::vector<InventoryActionData> &actions) {
    stream.putArrayLength((uint32_t) actions.size());
    for (const InventoryActionData &action: actions) {
        writeInventorySource(stream, action.mSource);
        stream.putUnsignedVarInt((uint32_t) action.mSlot);
        ItemCodec::writeNetworkItemStackDescriptor(stream, context, action.mFromItem);
        ItemCodec::writeNetworkItemStackDescriptor(stream, context, action.mToItem);
    }
}

void InventoryCodec::readInventoryActions(ReadOnlyBinaryStream &stream, const PacketCodecContext &context,
                                           std::vector<InventoryActionData> &actions) {
    uint32_t length = stream.getArrayLength();
    actions.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        InventoryActionData action;
        action.mSource = readInventorySource(stream);
        action.mSlot = (int32_t) stream.getUnsignedVarInt();
        action.mFromItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
        action.mToItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
        actions.push_back(std::move(action));
    }
}

void InventoryCodec::writeStackRequestSlotInfo(BinaryStream &stream, const ItemStackRequestSlotData &data) {
    writeFullContainerName(stream, data.mContainerName);
    stream.putByte((unsigned char) data.mSlot);
    stream.putLInt((uint32_t) data.mStackNetworkId);
}

ItemStackRequestSlotData InventoryCodec::readStackRequestSlotInfo(ReadOnlyBinaryStream &stream) {
    ItemStackRequestSlotData data;
    data.mContainerName = readFullContainerName(stream);
    data.mContainer = data.mContainerName.mContainer;
    data.mSlot = stream.getByte();
    data.mStackNetworkId = (int32_t) stream.getLInt();
    return data;
}

namespace {

    void writeRequestActionData(BinaryStream &stream, const ItemStackRequestAction &action) {
        stream.putByte((unsigned char) itemStackRequestActionJavaOrdinal(action.mType));

        switch (action.mType) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
                stream.putByte((unsigned char) action.mCount);
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mSource);
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mDestination);
                break;
            case ItemStackRequestActionType::Swap:
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mSource);
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mDestination);
                break;
            case ItemStackRequestActionType::Drop:
                stream.putByte((unsigned char) action.mCount);
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mSource);
                stream.putBool(action.mRandomly);
                break;
            case ItemStackRequestActionType::Destroy:
                stream.putByte((unsigned char) action.mCount);
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mSource);
                break;
            case ItemStackRequestActionType::Consume:
                stream.putByte((unsigned char) action.mCount);
                InventoryCodec::writeStackRequestSlotInfo(stream, action.mSource);
                break;
            case ItemStackRequestActionType::Create:
                stream.putByte((unsigned char) action.mSlot);
                break;
            case ItemStackRequestActionType::LabTableCombine:
                break;
            case ItemStackRequestActionType::BeaconPayment:
                stream.putVarInt(action.mPrimaryEffect);
                stream.putVarInt(action.mSecondaryEffect);
                break;
            case ItemStackRequestActionType::MineBlock:
                stream.putVarInt(action.mHotbarSlot);
                stream.putVarInt(action.mPredictedDurability);
                stream.putLInt((uint32_t) action.mStackNetworkId);
                break;
            case ItemStackRequestActionType::CraftRecipe:
                stream.putVarInt(action.mRecipeNetworkId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                break;
            case ItemStackRequestActionType::CraftCreative:
                stream.putUnsignedVarInt((uint32_t) action.mCreativeItemNetworkId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                break;
            case ItemStackRequestActionType::CraftRecipeOptional:
                stream.putVarInt(action.mRecipeNetworkId);
                stream.putLInt((uint32_t) action.mFilteredStringIndex);
                break;
            case ItemStackRequestActionType::CraftRepairAndDisenchant:
                stream.putVarInt(action.mRecipeNetworkId);
                stream.putVarInt(action.mRepairCost);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                break;
            case ItemStackRequestActionType::CraftLoom:
                stream.putString(action.mPatternId);
                stream.putByte((unsigned char) action.mTimesCrafted);
                break;
            case ItemStackRequestActionType::CraftNonImplemented:
                break;
            case ItemStackRequestActionType::CraftRecipeAuto:
            case ItemStackRequestActionType::CraftResultsDeprecated:
                throw BinaryDataException("ItemStackRequestAction crafting-ingredient encoding is not supported yet");
        }
    }

    ItemStackRequestAction readRequestActionData(ReadOnlyBinaryStream &stream, ItemStackRequestActionType type) {
        stream.getByte();

        ItemStackRequestAction action;
        action.mType = type;

        switch (type) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
                action.mCount = stream.getByte();
                action.mSource = InventoryCodec::readStackRequestSlotInfo(stream);
                action.mDestination = InventoryCodec::readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Swap:
                action.mSource = InventoryCodec::readStackRequestSlotInfo(stream);
                action.mDestination = InventoryCodec::readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Drop:
                action.mCount = stream.getByte();
                action.mSource = InventoryCodec::readStackRequestSlotInfo(stream);
                action.mRandomly = stream.getBool();
                break;
            case ItemStackRequestActionType::Destroy:
                action.mCount = stream.getByte();
                action.mSource = InventoryCodec::readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Consume:
                action.mCount = stream.getByte();
                action.mSource = InventoryCodec::readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Create:
                action.mSlot = stream.getByte();
                break;
            case ItemStackRequestActionType::LabTableCombine:
                break;
            case ItemStackRequestActionType::BeaconPayment:
                action.mPrimaryEffect = stream.getVarInt();
                action.mSecondaryEffect = stream.getVarInt();
                break;
            case ItemStackRequestActionType::MineBlock:
                action.mHotbarSlot = stream.getVarInt();
                action.mPredictedDurability = stream.getVarInt();
                action.mStackNetworkId = (int32_t) stream.getLInt();
                break;
            case ItemStackRequestActionType::CraftRecipe:
                action.mRecipeNetworkId = stream.getVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftCreative:
                action.mCreativeItemNetworkId = (int32_t) stream.getUnsignedVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftRecipeOptional:
                action.mRecipeNetworkId = stream.getVarInt();
                action.mFilteredStringIndex = (int32_t) stream.getLInt();
                break;
            case ItemStackRequestActionType::CraftRepairAndDisenchant:
                action.mRecipeNetworkId = stream.getVarInt();
                action.mRepairCost = stream.getVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftLoom:
                action.mPatternId = stream.getString();
                action.mTimesCrafted = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftNonImplemented:
                break;
            case ItemStackRequestActionType::CraftRecipeAuto: {
                action.mRecipeNetworkId = stream.getVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();

                const uint32_t ingredientCount = stream.getUnsignedVarInt();
                for (uint32_t i = 0; i < ingredientCount; i++)
                    skipRecipeIngredient(stream);

                break;
            }
            case ItemStackRequestActionType::CraftResultsDeprecated:
                throw BinaryDataException("ItemStackRequestAction crafting-ingredient decoding is not supported yet");
        }

        return action;
    }

}

void InventoryCodec::writeItemStackRequest(BinaryStream &stream, const PacketCodecContext &context, const ItemStackRequest &request) {
    stream.putVarInt(request.mRequestId);

    stream.putArrayLength((uint32_t) request.mActions.size());
    for (const ItemStackRequestAction &action: request.mActions) {
        stream.putUnsignedVarInt((uint32_t) itemStackRequestActionTypeToId(action.mType));
        writeRequestActionData(stream, action);
    }

    stream.putArrayLength((uint32_t) request.mFilterStrings.size());
    for (const std::string &filterString: request.mFilterStrings) {
        stream.putString(filterString);
    }

    stream.putLInt((uint32_t) (request.mHasTextProcessingEventOrigin ? request.mTextProcessingEventOrigin : -1));
}

ItemStackRequest InventoryCodec::readItemStackRequest(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    ItemStackRequest request;
    request.mRequestId = stream.getVarInt();

    uint32_t actionCount = stream.getArrayLength();
    request.mActions.reserve(actionCount);
    for (uint32_t i = 0; i < actionCount; i++) {
        ItemStackRequestActionType type = itemStackRequestActionTypeFromId((int32_t) stream.getUnsignedVarInt());
        request.mActions.push_back(readRequestActionData(stream, type));
    }

    uint32_t filterStringCount = stream.getArrayLength();
    request.mFilterStrings.reserve(filterStringCount);
    for (uint32_t i = 0; i < filterStringCount; i++) {
        request.mFilterStrings.push_back(stream.getString());
    }

    int32_t origin = (int32_t) stream.getLInt();
    request.mHasTextProcessingEventOrigin = origin != -1;
    request.mTextProcessingEventOrigin = origin;

    return request;
}

void InventoryCodec::writeItemUseTransaction(BinaryStream &stream, const PacketCodecContext &context, const ItemUseTransaction &transaction) {
    stream.putVarInt(transaction.mLegacyRequestId);
    stream.putBool(false);

    writeInventoryActions(stream, context, transaction.mActions);

    stream.putVarInt(transaction.mActionType);
    stream.putByte((unsigned char) transaction.mTriggerType);
    stream.putBlockPosition(transaction.mBlockPosition);
    stream.putByte((unsigned char) transaction.mBlockFace);
    stream.putVarInt(transaction.mHotbarSlot);
    stream.putByte((unsigned char) transaction.mHand);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, transaction.mItemInHand);
    stream.putVector3f(transaction.mPlayerPosition);
    stream.putVector3f(transaction.mClickPosition);
    stream.putUnsignedVarInt(transaction.mBlockDefinition == nullptr ? 0 : (uint32_t) transaction.mBlockDefinition->getRuntimeId());
    stream.putByte((unsigned char) transaction.mClientInteractPrediction);
    stream.putByte((unsigned char) transaction.mClientCooldownState);
}

ItemUseTransaction InventoryCodec::readItemUseTransaction(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    ItemUseTransaction transaction;

    transaction.mLegacyRequestId = stream.getVarInt();
    if (stream.getBool()) {
        const uint32_t changedSlotSetCount = stream.getUnsignedVarInt();
        for (uint32_t i = 0; i < changedSlotSetCount; i++) {
            stream.getByte();
            const uint32_t changedSlotCount = stream.getUnsignedVarInt();
            for (uint32_t j = 0; j < changedSlotCount; j++)
                stream.getByte();
        }
    }

    readInventoryActions(stream, context, transaction.mActions);

    transaction.mActionType = stream.getVarInt();
    transaction.mTriggerType = (ItemUseTriggerType) stream.getByte();
    transaction.mBlockPosition = stream.getBlockPosition();
    transaction.mBlockFace = stream.getByte();
    transaction.mHotbarSlot = stream.getVarInt();
    transaction.mHand = (HandSlot) stream.getByte();
    transaction.mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    transaction.mPlayerPosition = stream.getVector3f();
    transaction.mClickPosition = stream.getVector3f();

    int blockRuntimeId = (int) stream.getUnsignedVarInt();
    if (blockRuntimeId != 0) {
        transaction.mBlockDefinition = context.getBlockDefinitions().getDefinition(blockRuntimeId);
    }

    transaction.mClientInteractPrediction = (ItemUsePredictedResult) stream.getByte();
    transaction.mClientCooldownState = stream.getByte();

    return transaction;
}

void InventoryCodec::writePlayerBlockActionData(BinaryStream &stream, const PlayerBlockActionData &data) {
    stream.putVarInt((int32_t) data.mAction);
    stream.putBlockPosition(data.mBlockPosition);
    stream.putVarInt(data.mFace);
}

PlayerBlockActionData InventoryCodec::readPlayerBlockActionData(ReadOnlyBinaryStream &stream) {
    PlayerBlockActionData data;
    data.mAction = (PlayerActionType) stream.getVarInt();
    data.mBlockPosition = stream.getBlockPosition();
    data.mFace = stream.getVarInt();
    return data;
}
