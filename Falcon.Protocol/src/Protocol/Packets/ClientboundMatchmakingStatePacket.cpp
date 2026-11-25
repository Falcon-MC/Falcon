#include "Protocol/Packets/ClientboundMatchmakingStatePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundMatchmakingStatePacket::ClientboundMatchmakingStatePacket() = default;

void ClientboundMatchmakingStatePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mState);
    stream.putString(mDestinationName);

    stream.putOptionalPresent(mHasOptions);
    if (mHasOptions) {
        stream.putOptionalPresent(mHasTriggeringPlayerName);
        if (mHasTriggeringPlayerName)
            stream.putString(mTriggeringPlayerName);

        stream.putOptionalPresent(mHasTriggeredByLocalPlayer);
        if (mHasTriggeredByLocalPlayer)
            stream.putBool(mTriggeredByLocalPlayer);
    }
}

void ClientboundMatchmakingStatePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mState = (State) stream.getByte();
    mDestinationName = stream.getString();

    mHasOptions = stream.getOptionalPresent();
    mHasTriggeringPlayerName = false;
    mHasTriggeredByLocalPlayer = false;
    if (mHasOptions) {
        mHasTriggeringPlayerName = stream.getOptionalPresent();
        if (mHasTriggeringPlayerName)
            mTriggeringPlayerName = stream.getString();

        mHasTriggeredByLocalPlayer = stream.getOptionalPresent();
        if (mHasTriggeredByLocalPlayer)
            mTriggeredByLocalPlayer = stream.getBool();
    }
}

void ClientboundMatchmakingStatePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
