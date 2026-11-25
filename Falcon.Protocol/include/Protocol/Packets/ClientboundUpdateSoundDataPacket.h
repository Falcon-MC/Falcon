#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/SoundDataTypes.h"

#include <cstdint>

class ClientboundUpdateSoundDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundUpdateSoundData;

    ClientboundUpdateSoundDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundUpdateSoundDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mServerSoundHandle = 0;

    SoundDataUpdateType mType = SoundDataUpdateType::Stop;

    SetVolumeSoundData mVolume;
    SetPitchSoundData mPitch;
    FadeSoundData mFade;
    SeekToSoundData mSeekTo;

private:
    /**
     * Versions before the update type sent every update one after the other.
     */
    void writeEveryUpdate(BinaryStream &stream) const;

    void readEveryUpdate(ReadOnlyBinaryStream &stream);
};
