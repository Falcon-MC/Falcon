#include "Protocol/Packets/PlayerAuthInputPacket.h"

#include "Core/Utility/BinaryDataException.h"

#include <string>

#include "Protocol/InventoryCodec.h"
#include "Protocol/NetworkPacketHandler.h"

PlayerAuthInputPacket::PlayerAuthInputPacket() = default;

bool PlayerAuthInputPacket::hasInputFlag(int32_t flag) const {
    for (int32_t entry: mInputData) {
        if (entry == flag) {
            return true;
        }
    }
    return false;
}

void PlayerAuthInputPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLFloat(mRotation.x);
    stream.putLFloat(mRotation.y);
    stream.putVector3f(mPosition);
    stream.putLFloat(mMotionX);
    stream.putLFloat(mMotionY);
    stream.putLFloat(mRotation.z);

    if (context.getCapabilities().mDoubledPresence)
        stream.putBool(true);
    stream.putUnsignedVarInt((uint32_t) mInputData.size());
    for (int32_t entry: mInputData) {
        stream.putVarInt(entry);
    }

    stream.putUnsignedVarInt((uint32_t) mInputMode);
    stream.putUnsignedVarInt((uint32_t) mPlayMode);
    stream.putVarInt((int32_t) mInputInteractionModel);
    stream.putLFloat(mInteractRotationX);
    stream.putLFloat(mInteractRotationY);
    stream.putUnsignedVarLong((uint64_t) mTick);
    stream.putVector3f(mDelta);

    const bool itemInteraction = hasInputFlag((int32_t) PlayerAuthInputData::PerformItemInteraction);
    context.putPresence(stream, itemInteraction);
    if (itemInteraction)
        InventoryCodec::writeItemUseTransaction(stream, context, mItemUseTransaction);

    const bool stackRequest = hasInputFlag((int32_t) PlayerAuthInputData::PerformItemStackRequest);
    context.putPresence(stream, stackRequest);
    if (stackRequest)
        InventoryCodec::writeItemStackRequest(stream, context, mItemStackRequest);

    const bool blockActions = hasInputFlag((int32_t) PlayerAuthInputData::PerformBlockActions);
    context.putPresence(stream, blockActions);
    if (blockActions) {
        stream.putUnsignedVarInt((uint32_t) mPlayerActions.size());
        for (const PlayerBlockActionData &action: mPlayerActions) {
            InventoryCodec::writePlayerBlockActionData(stream, action);
        }
    }

    const bool inVehicle = hasInputFlag((int32_t) PlayerAuthInputData::InClientPredictedInVehicle);
    context.putPresence(stream, inVehicle);
    if (inVehicle) {
        stream.putLFloat(mVehicleRotationX);
        stream.putLFloat(mVehicleRotationY);
    }

    context.putPresence(stream, inVehicle);
    if (inVehicle)
        stream.putVarLong(mPredictedVehicle);

    stream.putLFloat(mAnalogMoveVectorX);
    stream.putLFloat(mAnalogMoveVectorY);
    stream.putVector3f(mCameraOrientation);
    stream.putLFloat(mRawMoveVectorX);
    stream.putLFloat(mRawMoveVectorY);
}

void PlayerAuthInputPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    float rotationX = stream.getLFloat();
    float rotationY = stream.getLFloat();
    mPosition = stream.getVector3f();
    mMotionX = stream.getLFloat();
    mMotionY = stream.getLFloat();
    float rotationZ = stream.getLFloat();
    mRotation = Vector3f(rotationX, rotationY, rotationZ);

    if (!context.getCapabilities().mDoubledPresence || stream.getBool()) {
        uint32_t inputCount = stream.getUnsignedVarInt();
        // an entry is a byte at least: a count the packet cannot hold must not size the allocation
        if (inputCount > stream.getRemainingLength())
            throw BinaryDataException("Input data count " + std::to_string(inputCount) + " exceeds the packet");
        mInputData.reserve(inputCount);
        for (uint32_t i = 0; i < inputCount; i++) {
            mInputData.push_back(stream.getVarInt());
        }
    }

    mInputMode = (PlayerInputMode) stream.getUnsignedVarInt();
    mPlayMode = (PlayerClientPlayMode) stream.getUnsignedVarInt();
    mInputInteractionModel = (PlayerInputInteractionModel) stream.getVarInt();
    mInteractRotationX = stream.getLFloat();
    mInteractRotationY = stream.getLFloat();
    mTick = (int64_t) stream.getUnsignedVarLong();
    mDelta = stream.getVector3f();

    if (context.getPresence(stream)) {
        mHasItemUseTransaction = true;
        mItemUseTransaction = InventoryCodec::readItemUseTransaction(stream, context);
    }

    if (context.getPresence(stream)) {
        mHasItemStackRequest = true;
        mItemStackRequest = InventoryCodec::readItemStackRequest(stream, context);
    }

    if (context.getPresence(stream)) {
        uint32_t count = stream.getUnsignedVarInt();
        // an action, three coordinates and a face, a byte each at least
        if (count > stream.getRemainingLength() / 5)
            throw BinaryDataException("Player action count " + std::to_string(count) + " exceeds the packet");
        mPlayerActions.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            mPlayerActions.push_back(InventoryCodec::readPlayerBlockActionData(stream));
        }
    }

    if (context.getPresence(stream)) {
        mHasVehicleRotation = true;
        mVehicleRotationX = stream.getLFloat();
        mVehicleRotationY = stream.getLFloat();
    }

    if (context.getPresence(stream)) {
        mHasPredictedVehicle = true;
        mPredictedVehicle = stream.getVarLong();
    }

    mAnalogMoveVectorX = stream.getLFloat();
    mAnalogMoveVectorY = stream.getLFloat();
    mCameraOrientation = stream.getVector3f();
    mRawMoveVectorX = stream.getLFloat();
    mRawMoveVectorY = stream.getLFloat();
}

void PlayerAuthInputPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
