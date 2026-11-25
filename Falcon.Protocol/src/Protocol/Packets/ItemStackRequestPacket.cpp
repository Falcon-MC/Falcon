#include "Protocol/Packets/ItemStackRequestPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"
#include "Protocol/Types/ContainerSlotType.h"
#include "Protocol/Types/FullContainerName.h"

namespace {
    void writeFullContainerName(BinaryStream &stream, const FullContainerName &name) {
        stream.putByte((unsigned char) containerSlotTypeToId(name.mContainer));
        stream.putOptionalPresent(name.mHasDynamicId);
        if (name.mHasDynamicId) {
            stream.putLInt((uint32_t) name.mDynamicId);
        }
    }

    FullContainerName readFullContainerName(ReadOnlyBinaryStream &stream) {
        FullContainerName name;
        name.mContainer = containerSlotTypeFromId(stream.getByte());
        name.mHasDynamicId = stream.getOptionalPresent();
        if (name.mHasDynamicId) {
            name.mDynamicId = (int32_t) stream.getLInt();
        }
        return name;
    }

    void writeStackRequestSlotInfo(BinaryStream &stream, const ItemStackRequestSlotData &data) {
        writeFullContainerName(stream, data.mContainerName);
        stream.putByte((unsigned char) data.mSlot);
        stream.putLInt((uint32_t) data.mStackNetworkId);
    }

    ItemStackRequestSlotData readStackRequestSlotInfo(ReadOnlyBinaryStream &stream) {
        ItemStackRequestSlotData data;
        data.mContainerName = readFullContainerName(stream);
        data.mContainer = data.mContainerName.mContainer;
        data.mSlot = stream.getByte();
        data.mStackNetworkId = (int32_t) stream.getLInt();
        return data;
    }

    void skipIngredient2(ReadOnlyBinaryStream &stream) {
        int32_t type = (int32_t) stream.getUnsignedVarInt();
        stream.getByte();

        switch (type) {
            case 1:
                stream.getString();
                stream.getVarInt();
                break;
            case 2:
                stream.getString();
                stream.getLShort();
                break;
            case 3:
                stream.getString();
                break;
            default:
                break;
        }

        stream.getLShort();
    }

    void writeRequestActionData(BinaryStream &stream, const PacketCodecContext &context, const ItemStackRequestAction &action) {
        switch (action.mType) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
                stream.putByte((unsigned char) action.mCount);
                writeStackRequestSlotInfo(stream, action.mSource);
                writeStackRequestSlotInfo(stream, action.mDestination);
                break;
            case ItemStackRequestActionType::Swap:
                writeStackRequestSlotInfo(stream, action.mSource);
                writeStackRequestSlotInfo(stream, action.mDestination);
                break;
            case ItemStackRequestActionType::Drop:
                stream.putByte((unsigned char) action.mCount);
                writeStackRequestSlotInfo(stream, action.mSource);
                stream.putBool(action.mRandomly);
                break;
            case ItemStackRequestActionType::Destroy:
            case ItemStackRequestActionType::Consume:
                stream.putByte((unsigned char) action.mCount);
                writeStackRequestSlotInfo(stream, action.mSource);
                break;
            case ItemStackRequestActionType::Create:
                stream.putByte((unsigned char) action.mSlot);
                break;
            case ItemStackRequestActionType::LabTableCombine:
            case ItemStackRequestActionType::CraftNonImplemented:
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
                stream.putUnsignedVarInt((uint32_t) action.mRecipeNetworkId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                break;
            case ItemStackRequestActionType::CraftRecipeAuto:
                stream.putUnsignedVarInt((uint32_t) action.mRecipeNetworkId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                stream.putArrayLength(0);
                break;
            case ItemStackRequestActionType::CraftCreative:
                stream.putUnsignedVarInt((uint32_t) action.mCreativeItemNetworkId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                break;
            case ItemStackRequestActionType::CraftRecipeOptional:
                stream.putUnsignedVarInt((uint32_t) action.mRecipeNetworkId);
                stream.putLInt((uint32_t) action.mFilteredStringIndex);
                break;
            case ItemStackRequestActionType::CraftRepairAndDisenchant:
                stream.putLInt((uint32_t) action.mRecipeNetworkId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                stream.putVarInt(action.mRepairCost);
                break;
            case ItemStackRequestActionType::CraftLoom:
                stream.putString(action.mPatternId);
                stream.putByte((unsigned char) action.mTimesCrafted);
                break;
            case ItemStackRequestActionType::CraftReserved:
                stream.putString(action.mReservedId);
                stream.putByte((unsigned char) action.mNumberOfRequestedCrafts);
                break;
            case ItemStackRequestActionType::CraftResultsDeprecated:
                stream.putArrayLength((uint32_t) action.mResultItems.size());
                for (const ItemStack &item: action.mResultItems) {
                    ItemCodec::writeRequestItemDescriptor(stream, context, item);
                }
                stream.putByte((unsigned char) action.mTimesCrafted);
                break;
        }
    }

    ItemStackRequestAction readRequestActionData(ReadOnlyBinaryStream &stream, const PacketCodecContext &context,
                                                 ItemStackRequestActionType type) {
        ItemStackRequestAction action;
        action.mType = type;

        switch (type) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
                action.mCount = stream.getByte();
                action.mSource = readStackRequestSlotInfo(stream);
                action.mDestination = readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Swap:
                action.mSource = readStackRequestSlotInfo(stream);
                action.mDestination = readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Drop:
                action.mCount = stream.getByte();
                action.mSource = readStackRequestSlotInfo(stream);
                action.mRandomly = stream.getBool();
                break;
            case ItemStackRequestActionType::Destroy:
            case ItemStackRequestActionType::Consume:
                action.mCount = stream.getByte();
                action.mSource = readStackRequestSlotInfo(stream);
                break;
            case ItemStackRequestActionType::Create:
                action.mSlot = stream.getByte();
                break;
            case ItemStackRequestActionType::LabTableCombine:
            case ItemStackRequestActionType::CraftNonImplemented:
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
                action.mRecipeNetworkId = (int32_t) stream.getUnsignedVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftRecipeAuto: {
                action.mRecipeNetworkId = (int32_t) stream.getUnsignedVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                uint32_t arrayLength = stream.getArrayLength();
                for (uint32_t i = 0; i < arrayLength; i++) {
                    skipIngredient2(stream);
                }
                break;
            }
            case ItemStackRequestActionType::CraftCreative:
                action.mCreativeItemNetworkId = (int32_t) stream.getUnsignedVarInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftRecipeOptional:
                action.mRecipeNetworkId = (int32_t) stream.getUnsignedVarInt();
                action.mFilteredStringIndex = (int32_t) stream.getLInt();
                break;
            case ItemStackRequestActionType::CraftRepairAndDisenchant:
                action.mRecipeNetworkId = (int32_t) stream.getLInt();
                action.mNumberOfRequestedCrafts = stream.getByte();
                action.mRepairCost = stream.getVarInt();
                break;
            case ItemStackRequestActionType::CraftLoom:
                action.mPatternId = stream.getString();
                action.mTimesCrafted = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftReserved:
                action.mReservedId = stream.getString();
                action.mNumberOfRequestedCrafts = stream.getByte();
                break;
            case ItemStackRequestActionType::CraftResultsDeprecated: {
                uint32_t count = stream.getArrayLength();
                action.mResultItems.reserve(count);
                for (uint32_t i = 0; i < count; i++) {
                    action.mResultItems.push_back(ItemCodec::readRequestItemDescriptor(stream, context));
                }
                action.mTimesCrafted = stream.getByte();
                break;
            }
        }

        return action;
    }
}

ItemStackRequestPacket::ItemStackRequestPacket() = default;

void ItemStackRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mRequests.size());
    for (const ItemStackRequest &request: mRequests) {
        stream.putVarInt(request.mRequestId);

        stream.putArrayLength((uint32_t) request.mActions.size());
        for (const ItemStackRequestAction &action: request.mActions) {
            const int32_t typeId = itemStackRequestActionIdForVersion(itemStackRequestActionTypeToId(action.mType),
                                                                      context.getCapabilities().mCraftReservedAction);
            stream.putUnsignedVarInt((uint32_t) typeId);
            stream.putByte((unsigned char) itemStackRequestActionTypeToLegacyId(typeId));
            writeRequestActionData(stream, context, action);
        }

        stream.putArrayLength((uint32_t) request.mFilterStrings.size());
        for (const std::string &filterString: request.mFilterStrings) {
            stream.putString(filterString);
        }

        stream.putLInt((uint32_t)(request.mHasTextProcessingEventOrigin ? request.mTextProcessingEventOrigin : -1));
    }
}

void ItemStackRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t requestCount = stream.getArrayLength();
    mRequests.reserve(requestCount);
    for (uint32_t i = 0; i < requestCount; i++) {
        ItemStackRequest request;
        request.mRequestId = stream.getVarInt();

        uint32_t actionCount = stream.getArrayLength();
        request.mActions.reserve(actionCount);
        for (uint32_t j = 0; j < actionCount; j++) {
            ItemStackRequestActionType type = itemStackRequestActionTypeFromId(itemStackRequestActionIdFromVersion(
                    (int32_t) stream.getUnsignedVarInt(), context.getCapabilities().mCraftReservedAction));
            stream.getByte();
            request.mActions.push_back(readRequestActionData(stream, context, type));
        }

        uint32_t filterCount = stream.getArrayLength();
        request.mFilterStrings.reserve(filterCount);
        for (uint32_t j = 0; j < filterCount; j++) {
            request.mFilterStrings.push_back(stream.getString());
        }

        int32_t origin = (int32_t) stream.getLInt();
        request.mHasTextProcessingEventOrigin = origin != -1;
        request.mTextProcessingEventOrigin = origin;

        mRequests.push_back(request);
    }
}

void ItemStackRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
