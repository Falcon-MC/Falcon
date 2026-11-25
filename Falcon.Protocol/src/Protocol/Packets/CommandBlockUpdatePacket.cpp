#include "Protocol/Packets/CommandBlockUpdatePacket.h"

#include "Protocol/NetworkPacketHandler.h"

CommandBlockUpdatePacket::CommandBlockUpdatePacket() = default;

void CommandBlockUpdatePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mBlock);

    if (mBlock) {
        stream.putBlockPosition(mBlockPosition);
        stream.putUnsignedVarInt((uint32_t) mMode);
        stream.putBool(mRedstoneMode);
        stream.putBool(mConditional);
    } else {
        stream.putUnsignedVarLong(mMinecartRuntimeActorId);
    }

    stream.putString(mCommand);
    stream.putString(mLastOutput);
    stream.putString(mName);
    stream.putString(mFilteredName);
    stream.putBool(mOutputTracked);
    stream.putLInt(mTickDelay);
    stream.putBool(mExecutingOnFirstTick);
}

void CommandBlockUpdatePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlock = stream.getBool();

    if (mBlock) {
        mBlockPosition = stream.getBlockPosition();
        mMode = (CommandBlockMode) stream.getUnsignedVarInt();
        mRedstoneMode = stream.getBool();
        mConditional = stream.getBool();
    } else {
        mMinecartRuntimeActorId = stream.getUnsignedVarLong();
    }

    mCommand = stream.getString();
    mLastOutput = stream.getString();
    mName = stream.getString();
    mFilteredName = stream.getString();
    mOutputTracked = stream.getBool();
    mTickDelay = stream.getLInt();
    mExecutingOnFirstTick = stream.getBool();
}

void CommandBlockUpdatePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
