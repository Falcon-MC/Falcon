#include "Protocol/Packets/InventoryTransactionPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"
#include "Protocol/Types/BlockDefinition.h"
#include "Protocol/Types/InventorySource.h"

namespace {
    void writeInventorySource(BinaryStream &stream, const InventorySource &source) {
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

        switch (source.mType) {
            case InventorySourceType::WorldInteraction:
                stream.putBool(true);
                stream.putUnsignedVarInt((uint32_t) source.mFlag);
                break;
            default:
                stream.putBool(false);
                break;
        }
    }

    InventorySource readInventorySource(ReadOnlyBinaryStream &stream) {
        InventorySource source;
        source.mType = inventorySourceTypeFromId((int32_t) stream.getUnsignedVarInt());

        int32_t containerId = 0;
        bool hasContainerId = stream.getBool();
        if (hasContainerId) {
            containerId = stream.getByte();
        }

        InventorySourceFlag flag = InventorySourceFlag::None;
        bool hasFlag = stream.getBool();
        if (hasFlag) {
            flag = (InventorySourceFlag) stream.getUnsignedVarInt();
        }

        switch (source.mType) {
            case InventorySourceType::Container:
            case InventorySourceType::NonImplementedTodo:
            case InventorySourceType::UntrackedInteractionUi:
                source.mContainerId = containerId;
                break;
            case InventorySourceType::WorldInteraction:
                source.mFlag = flag;
                break;
            default:
                break;
        }

        return source;
    }

    void writeInventoryActions(BinaryStream &stream, const PacketCodecContext &context,
                               const std::vector<InventoryActionData> &actions) {
        stream.putArrayLength((uint32_t) actions.size());
        for (const InventoryActionData &action: actions) {
            writeInventorySource(stream, action.mSource);
            stream.putUnsignedVarInt((uint32_t) action.mSlot);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, action.mFromItem);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, action.mToItem);
        }
    }

    void readInventoryActions(ReadOnlyBinaryStream &stream, const PacketCodecContext &context,
                              std::vector<InventoryActionData> &actions) {
        uint32_t count = stream.getArrayLength();
        actions.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            InventoryActionData action;
            action.mSource = readInventorySource(stream);
            action.mSlot = (int32_t) stream.getUnsignedVarInt();
            action.mFromItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            action.mToItem = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            actions.push_back(action);
        }
    }
}

InventoryTransactionPacket::InventoryTransactionPacket() = default;

void InventoryTransactionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mLegacyRequestId);

    if (mLegacyRequestId < -1 && (mLegacyRequestId & 1) == 0) {
        stream.putBool(true);
        stream.putArrayLength((uint32_t) mLegacySlots.size());
        for (const LegacySetItemSlotData &slot: mLegacySlots) {
            stream.putByte((unsigned char) slot.mContainerId);
            stream.putByteArray(slot.mSlots);
        }
    } else {
        stream.putBool(false);
    }

    stream.putUnsignedVarInt((uint32_t) mTransactionType);

    writeInventoryActions(stream, context, mActions);

    switch (mTransactionType) {
        case InventoryTransactionType::ItemUse:
            stream.putVarInt(mActionType);
            stream.putByte((unsigned char) mTriggerType);
            stream.putBlockPosition(mBlockPosition);
            stream.putByte((unsigned char) mBlockFace);
            stream.putVarInt(mHotbarSlot);
            stream.putByte((unsigned char) mHand);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItemInHand);
            stream.putVector3f(mPlayerPosition);
            stream.putVector3f(mClickPosition);
            stream.putUnsignedVarInt(mBlockDefinition == nullptr ? 0 : (uint32_t) mBlockDefinition->getRuntimeId());
            stream.putByte((unsigned char) mClientInteractPrediction);
            stream.putByte((unsigned char) mClientCooldownState);
            break;
        case InventoryTransactionType::ItemUseOnEntity:
            stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
            stream.putVarInt(mActionType);
            stream.putVarInt(mHotbarSlot);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItemInHand);
            stream.putVector3f(mPlayerPosition);
            stream.putVector3f(mClickPosition);
            break;
        case InventoryTransactionType::ItemRelease:
            stream.putVarInt(mActionType);
            stream.putVarInt(mHotbarSlot);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItemInHand);
            stream.putVector3f(mHeadPosition);
            break;
        default:
            break;
    }
}

void InventoryTransactionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mLegacyRequestId = stream.getVarInt();

    if (stream.getBool()) {
        if (mLegacyRequestId < -1 && (mLegacyRequestId & 1) == 0) {
            uint32_t count = stream.getArrayLength();
            mLegacySlots.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                LegacySetItemSlotData slot;
                slot.mContainerId = stream.getByte();
                slot.mSlots = stream.getByteArray();
                mLegacySlots.push_back(slot);
            }
        }
    }

    mTransactionType = (InventoryTransactionType) stream.getUnsignedVarInt();

    readInventoryActions(stream, context, mActions);

    switch (mTransactionType) {
        case InventoryTransactionType::ItemUse:
            mActionType = stream.getVarInt();
            mTriggerType = (ItemUseTriggerType) stream.getByte();
            mBlockPosition = stream.getBlockPosition();
            mBlockFace = stream.getByte();
            mHotbarSlot = stream.getVarInt();
            mHand = (HandSlot) stream.getByte();
            mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            mPlayerPosition = stream.getVector3f();
            mClickPosition = stream.getVector3f();
            mBlockDefinition = context.getBlockDefinitions().getDefinition((int) stream.getUnsignedVarInt());
            mClientInteractPrediction = (ItemUsePredictedResult) stream.getByte();
            mClientCooldownState = stream.getSignedByte();
            break;
        case InventoryTransactionType::ItemUseOnEntity:
            mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
            mActionType = stream.getVarInt();
            mHotbarSlot = stream.getVarInt();
            mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            mPlayerPosition = stream.getVector3f();
            mClickPosition = stream.getVector3f();
            break;
        case InventoryTransactionType::ItemRelease:
            mActionType = stream.getVarInt();
            mHotbarSlot = stream.getVarInt();
            mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            mHeadPosition = stream.getVector3f();
            break;
        default:
            break;
    }
}

void InventoryTransactionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
