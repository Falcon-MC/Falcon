#include "Protocol/Packets/ResourcePackStackPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ResourcePackStackPacket::ResourcePackStackPacket()
        : mForcedToAccept(false), mExperimentsPreviouslyToggled(false), mEditorPackTypes(0) {}

void ResourcePackStackPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mForcedToAccept);

    stream.putArrayLength((uint32_t) mResourcePacks.size());
    for (const Entry &entry: mResourcePacks) {
        stream.putString(entry.mPackId);
        stream.putString(entry.mPackVersion);
        stream.putString(entry.mSubPackName);
    }

    stream.putString(mGameVersion);
    writeExperiments(stream, mExperiments);
    stream.putBool(mExperimentsPreviouslyToggled);
    if (context.getCapabilities().mEditorPackTypes)
        stream.putByte(mEditorPackTypes);
    else
        stream.putBool(mEditorPackTypes != 0);
}

void ResourcePackStackPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mForcedToAccept = stream.getBool();

    const uint32_t count = stream.getArrayLength();
    mResourcePacks.clear();
    mResourcePacks.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        Entry entry;
        entry.mPackId = stream.getString();
        entry.mPackVersion = stream.getString();
        entry.mSubPackName = stream.getString();
        mResourcePacks.push_back(entry);
    }

    mGameVersion = stream.getString();
    readExperiments(stream, mExperiments);
    mExperimentsPreviouslyToggled = stream.getBool();
    if (context.getCapabilities().mEditorPackTypes)
        mEditorPackTypes = stream.getByte();
    else
        mEditorPackTypes = stream.getBool() ? EditorPackType::CORE : 0;
}

void ResourcePackStackPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
