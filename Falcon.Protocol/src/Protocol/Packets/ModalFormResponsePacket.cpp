#include "Protocol/Packets/ModalFormResponsePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ModalFormResponsePacket::ModalFormResponsePacket()
        : mFormId(0), mHasFormData(false), mHasCancelReason(false), mCancelReason(CancelReason::UserClosed) {}

void ModalFormResponsePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt(mFormId);

    stream.putOptionalPresent(mHasFormData);
    if (mHasFormData)
        stream.putString(mFormData);

    stream.putOptionalPresent(mHasCancelReason);
    if (mHasCancelReason)
        stream.putByte((unsigned char) mCancelReason);
}

void ModalFormResponsePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mFormId = stream.getUnsignedVarInt();

    mHasFormData = stream.getOptionalPresent();
    if (mHasFormData)
        mFormData = stream.getString();

    mHasCancelReason = stream.getOptionalPresent();
    if (mHasCancelReason)
        mCancelReason = (CancelReason) stream.getByte();
}

void ModalFormResponsePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
