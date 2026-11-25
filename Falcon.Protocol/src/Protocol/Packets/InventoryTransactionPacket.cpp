#include "Protocol/Packets/InventoryTransactionPacket.h"

#include "Core/Utility/BinaryDataException.h"
#include "Protocol/InventoryCodec.h"
#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"
#include "Protocol/Types/BlockDefinition.h"
#include "Protocol/Types/InventorySource.h"

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

    const bool doubledPresence = context.getCapabilities().mDoubledPresence;
    if (doubledPresence)
        stream.putBool(true);
    stream.putUnsignedVarInt((uint32_t) mTransactionType);

    if (doubledPresence)
        stream.putBool(true);
    InventoryCodec::writeInventoryActions(stream, context, mActions);

    switch (mTransactionType) {
        case InventoryTransactionType::ItemUse:
            stream.putVarInt(mActionType);
            stream.putByte((unsigned char) mTriggerType);
            stream.putBlockPosition(mBlockPosition);
            stream.putByte((unsigned char) mBlockFace);
            stream.putVarInt(mHotbarSlot);
            if (context.getCapabilities().mItemUseHand)
                stream.putByte((unsigned char) mHand);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItemInHand);
            stream.putVector3f(mPlayerPosition);
            stream.putVector3f(mClickPosition);
            stream.putUnsignedVarInt(mBlockDefinition == nullptr
                                     ? 0 : (uint32_t) context.toNetworkBlockId(mBlockDefinition->getRuntimeId()));
            stream.putByte((unsigned char) mClientInteractPrediction);
            stream.putByte((unsigned char) mClientCooldownState);
            break;
        case InventoryTransactionType::ItemUseOnEntity:
            stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
            stream.putVarInt(mActionType);
            stream.putVarInt(mHotbarSlot);
            if (context.getCapabilities().mTransactionHandOnEveryUse)
                stream.putByte((unsigned char) mHand);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItemInHand);
            stream.putVector3f(mPlayerPosition);
            stream.putVector3f(mClickPosition);
            break;
        case InventoryTransactionType::ItemRelease:
            stream.putVarInt(mActionType);
            stream.putVarInt(mHotbarSlot);
            ItemCodec::writeNetworkItemStackDescriptor(stream, context, mItemInHand);
            stream.putVector3f(mHeadPosition);
            if (context.getCapabilities().mTransactionHandOnEveryUse)
                stream.putByte((unsigned char) mHand);
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

    const bool doubledPresence = context.getCapabilities().mDoubledPresence;
    if (doubledPresence && !stream.getBool())
        throw BinaryDataException("the inventory transaction type is missing");
    mTransactionType = (InventoryTransactionType) stream.getUnsignedVarInt();

    if (doubledPresence && !stream.getBool())
        throw BinaryDataException("the inventory actions are missing");
    InventoryCodec::readInventoryActions(stream, context, mActions);

    switch (mTransactionType) {
        case InventoryTransactionType::ItemUse:
            mActionType = stream.getVarInt();
            mTriggerType = (ItemUseTriggerType) stream.getByte();
            mBlockPosition = stream.getBlockPosition();
            mBlockFace = stream.getByte();
            mHotbarSlot = stream.getVarInt();
            if (context.getCapabilities().mItemUseHand)
                mHand = (HandSlot) stream.getByte();
            mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            mPlayerPosition = stream.getVector3f();
            mClickPosition = stream.getVector3f();
            mBlockDefinition = context.getBlockDefinitions().getDefinition(
                    context.fromNetworkBlockId((int) stream.getUnsignedVarInt()));
            mClientInteractPrediction = (ItemUsePredictedResult) stream.getByte();
            mClientCooldownState = stream.getSignedByte();
            break;
        case InventoryTransactionType::ItemUseOnEntity:
            mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
            mActionType = stream.getVarInt();
            mHotbarSlot = stream.getVarInt();
            if (context.getCapabilities().mTransactionHandOnEveryUse)
                mHand = (HandSlot) stream.getByte();
            mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            mPlayerPosition = stream.getVector3f();
            mClickPosition = stream.getVector3f();
            break;
        case InventoryTransactionType::ItemRelease:
            mActionType = stream.getVarInt();
            mHotbarSlot = stream.getVarInt();
            mItemInHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
            mHeadPosition = stream.getVector3f();
            if (context.getCapabilities().mTransactionHandOnEveryUse)
                mHand = (HandSlot) stream.getByte();
            break;
        default:
            break;
    }
}

void InventoryTransactionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
