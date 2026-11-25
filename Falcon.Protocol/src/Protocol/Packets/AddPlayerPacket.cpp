#include "Protocol/Packets/AddPlayerPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

AddPlayerPacket::AddPlayerPacket() = default;

void AddPlayerPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUuid(mUuid);
    stream.putString(mUsername);
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
    stream.putString(mPlatformChatId);
    stream.putVector3f(mPosition);
    stream.putVector3f(mMotion);
    stream.putVector3f(mRotation);
    ItemCodec::writeNetworkItemStackDescriptor(stream, context, mHand);
    stream.putVarInt(mGameType);
    EntityCodec::writeEntityData(stream, context, mMetadata);
    EntityCodec::writeEntityProperties(stream, mProperties);
    EntityCodec::writePlayerAbilities(stream, mAbilities);

    stream.putArrayLength((uint32_t) mActorLinks.size());
    for (const EntityLinkData &link: mActorLinks) {
        EntityCodec::writeEntityLink(stream, link);
    }

    if (context.getCapabilities().mPassengerOfBlock)
        EntityCodec::writePassengerOfBlock(stream, mHasPassengerOfBlock, mPassengerOfBlock);
    stream.putString(mDeviceId);
    stream.putLInt((uint32_t) mBuildPlatform);
}

void AddPlayerPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUuid = stream.getUuid();
    mUsername = stream.getString();
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
    mPlatformChatId = stream.getString();
    mPosition = stream.getVector3f();
    mMotion = stream.getVector3f();
    mRotation = stream.getVector3f();
    mHand = ItemCodec::readNetworkItemStackDescriptor(stream, context);
    mGameType = stream.getVarInt();
    mMetadata = EntityCodec::readEntityData(stream);
    mProperties = EntityCodec::readEntityProperties(stream);
    mAbilities = EntityCodec::readPlayerAbilities(stream);

    uint32_t linkCount = stream.getArrayLength();
    mActorLinks.reserve(linkCount);
    for (uint32_t i = 0; i < linkCount; i++) {
        mActorLinks.push_back(EntityCodec::readEntityLink(stream));
    }

    mHasPassengerOfBlock = context.getCapabilities().mPassengerOfBlock
                           && EntityCodec::readPassengerOfBlock(stream, mPassengerOfBlock);
    mDeviceId = stream.getString();
    mBuildPlatform = (int32_t) stream.getLInt();
}

void AddPlayerPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
