#include "Protocol/Packets/ClientboundMapItemDataPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundMapItemDataPacket::ClientboundMapItemDataPacket() = default;

void ClientboundMapItemDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueMapId);
    stream.putByte((unsigned char) mDimensionId);
    stream.putBool(mLocked);
    stream.putVector3i(mOrigin);

    stream.putOptionalPresent(mHasTrackedActorIds);
    if (mHasTrackedActorIds) {
        stream.putUnsignedVarInt((uint32_t) mTrackedActorIds.size());
        for (int64_t id: mTrackedActorIds) {
            stream.putVarLong(id);
        }
    }

    stream.putOptionalPresent(mHasScale);
    if (mHasScale) {
        stream.putByte((unsigned char) mScale);
    }

    stream.putOptionalPresent(mHasTrackedObjects);
    if (mHasTrackedObjects) {
        stream.putUnsignedVarInt((uint32_t) mTrackedObjects.size());
        for (const MapTrackedObject &object: mTrackedObjects) {
            stream.putLInt((uint32_t) object.mType);
            if (object.mType == MapTrackedObjectType::Entity) {
                stream.putBool(true);
                stream.putVarLong(object.mActorId);
                stream.putBool(false);
            } else {
                stream.putBool(false);
                stream.putBool(true);
                stream.putBlockPosition(object.mPosition);
            }
        }
    }

    stream.putOptionalPresent(mHasDecorations);
    if (mHasDecorations) {
        stream.putUnsignedVarInt((uint32_t) mDecorations.size());
        for (const MapDecoration &decoration: mDecorations) {
            stream.putByte((unsigned char) decoration.mImage);
            stream.putByte((unsigned char) decoration.mRotation);
            stream.putByte((unsigned char) decoration.mXOffset);
            stream.putByte((unsigned char) decoration.mYOffset);
            stream.putString(decoration.mLabel);
            stream.putLInt((uint32_t) decoration.mColor);
        }
    }

    stream.putOptionalPresent(mHasWidth);
    if (mHasWidth) {
        stream.putVarInt(mWidth);
    }

    stream.putOptionalPresent(mHasHeight);
    if (mHasHeight) {
        stream.putVarInt(mHeight);
    }

    stream.putOptionalPresent(mHasXOffset);
    if (mHasXOffset) {
        stream.putVarInt(mXOffset);
    }

    stream.putOptionalPresent(mHasYOffset);
    if (mHasYOffset) {
        stream.putVarInt(mYOffset);
    }

    stream.putOptionalPresent(mHasColors);
    if (mHasColors) {
        stream.putUnsignedVarInt((uint32_t) mColors.size());
        for (int32_t color: mColors) {
            stream.putLInt((uint32_t) color);
        }
    }
}

void ClientboundMapItemDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueMapId = stream.getVarLong();
    mDimensionId = stream.getByte();
    mLocked = stream.getBool();
    mOrigin = stream.getVector3i();

    mHasTrackedActorIds = stream.getBool();
    if (mHasTrackedActorIds) {
        uint32_t length = stream.getUnsignedVarInt();
        mTrackedActorIds.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            mTrackedActorIds.push_back(stream.getVarLong());
        }
    }

    mHasScale = stream.getOptionalPresent();
    if (mHasScale) {
        mScale = (int8_t) stream.getByte();
    }

    mHasTrackedObjects = stream.getBool();
    if (mHasTrackedObjects) {
        uint32_t length = stream.getUnsignedVarInt();
        mTrackedObjects.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            MapTrackedObject object;
            object.mType = (MapTrackedObjectType) stream.getLInt();
            if (stream.getBool()) {
                object.mActorId = stream.getVarLong();
            }
            if (stream.getBool()) {
                object.mPosition = stream.getBlockPosition();
            }
            mTrackedObjects.push_back(object);
        }
    }

    mHasDecorations = stream.getBool();
    if (mHasDecorations) {
        uint32_t length = stream.getUnsignedVarInt();
        mDecorations.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            MapDecoration decoration;
            decoration.mImage = (int8_t) stream.getByte();
            decoration.mRotation = stream.getByte();
            decoration.mXOffset = stream.getByte();
            decoration.mYOffset = stream.getByte();
            decoration.mLabel = stream.getString();
            decoration.mColor = (int32_t) stream.getLInt();
            mDecorations.push_back(decoration);
        }
    }

    mHasWidth = stream.getOptionalPresent();
    if (mHasWidth) {
        mWidth = stream.getVarInt();
    }

    mHasHeight = stream.getOptionalPresent();
    if (mHasHeight) {
        mHeight = stream.getVarInt();
    }

    mHasXOffset = stream.getOptionalPresent();
    if (mHasXOffset) {
        mXOffset = stream.getVarInt();
    }

    mHasYOffset = stream.getOptionalPresent();
    if (mHasYOffset) {
        mYOffset = stream.getVarInt();
    }

    mHasColors = stream.getBool();
    if (mHasColors) {
        uint32_t length = stream.getUnsignedVarInt();
        mColors.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            mColors.push_back((int32_t) stream.getLInt());
        }
    }
}

void ClientboundMapItemDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
