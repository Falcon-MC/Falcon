#include "Protocol/Packets/LocatorBarPacket.h"

#include "Protocol/NetworkPacketHandler.h"

LocatorBarPacket::LocatorBarPacket() = default;

void LocatorBarPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mWaypoints.size());
    for (const LocatorBarPayload &payload: mWaypoints) {
        stream.putUuid(payload.mGroupHandle);

        const LocatorBarWaypoint &waypoint = payload.mWaypoint;
        stream.putLInt((uint32_t) waypoint.mUpdateFlag);

        stream.putOptionalPresent(waypoint.mVisible.has_value());
        if (waypoint.mVisible.has_value())
            stream.putBool(waypoint.mVisible.value());

        stream.putOptionalPresent(waypoint.mWorldPosition.has_value());
        if (waypoint.mWorldPosition.has_value()) {
            stream.putVector3f(waypoint.mWorldPosition->mPosition);
            stream.putVarInt(waypoint.mWorldPosition->mDimension);
        }

        stream.putOptionalPresent(waypoint.mTexturePath.has_value());
        if (waypoint.mTexturePath.has_value())
            stream.putString(waypoint.mTexturePath.value());

        stream.putOptionalPresent(waypoint.mIconSize.has_value());
        if (waypoint.mIconSize.has_value())
            stream.putVector2f(waypoint.mIconSize.value());

        stream.putOptionalPresent(waypoint.mColor.has_value());
        if (waypoint.mColor.has_value())
            stream.putLInt((uint32_t) waypoint.mColor.value());

        stream.putOptionalPresent(waypoint.mClientPositionAuthority.has_value());
        if (waypoint.mClientPositionAuthority.has_value())
            stream.putBool(waypoint.mClientPositionAuthority.value());

        stream.putOptionalPresent(waypoint.mActorUniqueId.has_value());
        if (waypoint.mActorUniqueId.has_value())
            stream.putVarLong(waypoint.mActorUniqueId.value());

        stream.putByte((unsigned char) payload.mActionFlag);
    }
}

void LocatorBarPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mWaypoints.clear();
    uint32_t count = stream.getArrayLength();
    for (uint32_t i = 0; i < count; i++) {
        LocatorBarPayload payload;
        payload.mGroupHandle = stream.getUuid();

        LocatorBarWaypoint waypoint;
        waypoint.mUpdateFlag = (int32_t) stream.getLInt();

        if (stream.getOptionalPresent())
            waypoint.mVisible = stream.getBool();

        if (stream.getOptionalPresent()) {
            LocatorBarWorldPosition position;
            position.mPosition = stream.getVector3f();
            position.mDimension = stream.getVarInt();
            waypoint.mWorldPosition = position;
        }

        if (stream.getOptionalPresent())
            waypoint.mTexturePath = stream.getString();

        if (stream.getOptionalPresent())
            waypoint.mIconSize = stream.getVector2f();

        if (stream.getOptionalPresent())
            waypoint.mColor = (int32_t) stream.getLInt();

        if (stream.getOptionalPresent())
            waypoint.mClientPositionAuthority = stream.getBool();

        if (stream.getOptionalPresent())
            waypoint.mActorUniqueId = stream.getVarLong();

        payload.mWaypoint = waypoint;
        payload.mActionFlag = (LocatorBarAction) stream.getByte();

        mWaypoints.push_back(payload);
    }
}

void LocatorBarPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
