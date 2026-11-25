#include "Protocol/Packets/ResourcePacksInfoPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ResourcePacksInfoPacket::ResourcePacksInfoPacket()
        : mForcedToAccept(false), mHasAddonPacks(false), mScriptingEnabled(false),
          mVibrantVisualsForceDisabled(false) {}

void ResourcePacksInfoPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mForcedToAccept);
    stream.putBool(mHasAddonPacks);
    stream.putBool(mScriptingEnabled);
    stream.putBool(mVibrantVisualsForceDisabled);
    stream.putUuid(mWorldTemplateId);
    stream.putString(mWorldTemplateVersion);

    stream.putArrayLength((uint32_t) mResourcePackInfos.size());
    for (const Entry &entry: mResourcePackInfos) {
        stream.putUuid(entry.mPackId);
        stream.putString(entry.mPackVersion);
        stream.putLLong(entry.mPackSize);
        stream.putString(entry.mContentKey);
        stream.putString(entry.mSubPackName);
        stream.putString(entry.mContentId);
        stream.putBool(entry.mScripting);
        stream.putBool(entry.mAddonPack);
        stream.putBool(entry.mRaytracingCapable);
        stream.putString(entry.mCdnUrl);
    }
}

void ResourcePacksInfoPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mForcedToAccept = stream.getBool();
    mHasAddonPacks = stream.getBool();
    mScriptingEnabled = stream.getBool();
    mVibrantVisualsForceDisabled = stream.getBool();
    mWorldTemplateId = stream.getUuid();
    mWorldTemplateVersion = stream.getString();

    const uint32_t count = stream.getArrayLength();
    mResourcePackInfos.clear();
    mResourcePackInfos.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        Entry entry;
        entry.mPackId = stream.getUuid();
        entry.mPackVersion = stream.getString();
        entry.mPackSize = stream.getLLong();
        entry.mContentKey = stream.getString();
        entry.mSubPackName = stream.getString();
        entry.mContentId = stream.getString();
        entry.mScripting = stream.getBool();
        entry.mAddonPack = stream.getBool();
        entry.mRaytracingCapable = stream.getBool();
        entry.mCdnUrl = stream.getString();
        mResourcePackInfos.push_back(entry);
    }
}

void ResourcePacksInfoPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
