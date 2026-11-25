#include "Protocol/Packets/AddActorPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

AddActorPacket::AddActorPacket() = default;

void AddActorPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
    stream.putString(mIdentifier);
    stream.putVector3f(mPosition);
    stream.putVector3f(mMotion);
    stream.putVector2f(mRotation);
    stream.putLFloat(mHeadRotation);
    stream.putLFloat(mBodyRotation);

    stream.putArrayLength((uint32_t) mAttributes.size());
    for (const AttributeData &attribute: mAttributes) {
        stream.putString(attribute.mName);
        stream.putLFloat(attribute.mMinimum);
        stream.putLFloat(attribute.mValue);
        stream.putLFloat(attribute.mMaximum);
    }

    EntityCodec::writeEntityData(stream, context, mMetadata);
    EntityCodec::writeEntityProperties(stream, mProperties);

    stream.putArrayLength((uint32_t) mActorLinks.size());
    for (const EntityLinkData &link: mActorLinks) {
        EntityCodec::writeEntityLink(stream, link);
    }

    if (context.getCapabilities().mPassengerOfBlock)
        EntityCodec::writePassengerOfBlock(stream, mHasPassengerOfBlock, mPassengerOfBlock);
}

void AddActorPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
    mIdentifier = stream.getString();
    mPosition = stream.getVector3f();
    mMotion = stream.getVector3f();
    mRotation = stream.getVector2f();
    mHeadRotation = stream.getLFloat();
    mBodyRotation = stream.getLFloat();

    uint32_t attributeCount = stream.getArrayLength();
    mAttributes.reserve(attributeCount);
    for (uint32_t i = 0; i < attributeCount; i++) {
        AttributeData attribute;
        attribute.mName = stream.getString();
        attribute.mMinimum = stream.getLFloat();
        attribute.mValue = stream.getLFloat();
        attribute.mMaximum = stream.getLFloat();
        attribute.mDefaultMinimum = attribute.mMinimum;
        attribute.mDefaultMaximum = attribute.mMaximum;
        attribute.mDefaultValue = attribute.mValue;
        mAttributes.push_back(attribute);
    }

    mMetadata = EntityCodec::readEntityData(stream);
    mProperties = EntityCodec::readEntityProperties(stream);

    uint32_t linkCount = stream.getArrayLength();
    mActorLinks.reserve(linkCount);
    for (uint32_t i = 0; i < linkCount; i++) {
        mActorLinks.push_back(EntityCodec::readEntityLink(stream));
    }

    mHasPassengerOfBlock = context.getCapabilities().mPassengerOfBlock
                           && EntityCodec::readPassengerOfBlock(stream, mPassengerOfBlock);
}

void AddActorPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
