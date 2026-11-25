#include "Protocol/Packets/ServerPresenceInfoPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerPresenceInfoPacket::ServerPresenceInfoPacket() = default;

void ServerPresenceInfoPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putOptionalPresent(mHasPresenceConfiguration);
    if (!mHasPresenceConfiguration)
        return;

    stream.putOptionalPresent(mHasExperienceName);
    if (mHasExperienceName)
        stream.putString(mExperienceName);

    stream.putOptionalPresent(mHasWorldName);
    if (mHasWorldName)
        stream.putString(mWorldName);

    stream.putString(mRichPresenceId);
}

void ServerPresenceInfoPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHasPresenceConfiguration = stream.getOptionalPresent();
    if (!mHasPresenceConfiguration)
        return;

    mHasExperienceName = stream.getOptionalPresent();
    if (mHasExperienceName)
        mExperienceName = stream.getString();

    mHasWorldName = stream.getOptionalPresent();
    if (mHasWorldName)
        mWorldName = stream.getString();

    mRichPresenceId = stream.getString();
}

void ServerPresenceInfoPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
