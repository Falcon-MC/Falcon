#include "Protocol/Packets/PlayerAuthInputPacket.h"

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

    if (hasInputFlag((int32_t) PlayerAuthInputData::PerformItemInteraction)) {
        stream.putBool(true);
        InventoryCodec::writeItemUseTransaction(stream, context, mItemUseTransaction);
    } else {
        stream.putBool(false);
    }

    if (hasInputFlag((int32_t) PlayerAuthInputData::PerformItemStackRequest)) {
        stream.putBool(true);
        InventoryCodec::writeItemStackRequest(stream, context, mItemStackRequest);
    } else {
        stream.putBool(false);
    }

    if (hasInputFlag((int32_t) PlayerAuthInputData::PerformBlockActions)) {
        stream.putBool(true);
        stream.putUnsignedVarInt((uint32_t) mPlayerActions.size());
        for (const PlayerBlockActionData &action: mPlayerActions) {
            InventoryCodec::writePlayerBlockActionData(stream, action);
        }
    } else {
        stream.putBool(false);
    }

    if (hasInputFlag((int32_t) PlayerAuthInputData::InClientPredictedInVehicle)) {
        stream.putBool(true);
        stream.putLFloat(mVehicleRotationX);
        stream.putLFloat(mVehicleRotationY);
    } else {
        stream.putBool(false);
    }

    if (hasInputFlag((int32_t) PlayerAuthInputData::InClientPredictedInVehicle)) {
        stream.putBool(true);
        stream.putVarLong(mPredictedVehicle);
    } else {
        stream.putBool(false);
    }

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

    uint32_t inputCount = stream.getUnsignedVarInt();
    mInputData.reserve(inputCount);
    for (uint32_t i = 0; i < inputCount; i++) {
        mInputData.push_back(stream.getVarInt());
    }

    mInputMode = (PlayerInputMode) stream.getUnsignedVarInt();
    mPlayMode = (PlayerClientPlayMode) stream.getUnsignedVarInt();
    mInputInteractionModel = (PlayerInputInteractionModel) stream.getVarInt();
    mInteractRotationX = stream.getLFloat();
    mInteractRotationY = stream.getLFloat();
    mTick = (int64_t) stream.getUnsignedVarLong();
    mDelta = stream.getVector3f();

    if (stream.getBool()) {
        mHasItemUseTransaction = true;
        mItemUseTransaction = InventoryCodec::readItemUseTransaction(stream, context);
    }

    if (stream.getBool()) {
        mHasItemStackRequest = true;
        mItemStackRequest = InventoryCodec::readItemStackRequest(stream, context);
    }

    if (stream.getBool()) {
        uint32_t count = stream.getUnsignedVarInt();
        mPlayerActions.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            mPlayerActions.push_back(InventoryCodec::readPlayerBlockActionData(stream));
        }
    }

    if (stream.getBool()) {
        mHasVehicleRotation = true;
        mVehicleRotationX = stream.getLFloat();
        mVehicleRotationY = stream.getLFloat();
    }

    if (stream.getBool()) {
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
