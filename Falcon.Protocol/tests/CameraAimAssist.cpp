#include "Protocol/CameraCodec.h"
#include "Protocol/Packets/CameraPresetsPacket.h"

#include <stdexcept>

static void check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

int main() {
    const unsigned char bytes[] = {
        14, 'a', 'u', 'd', 'i', 't', ':', 'h', 'i', 't', 'b', 'o', 'x', 'e', 's', 1,
        0, 0, 240, 65, 0, 0, 240, 65, 0, 0, 32, 65
    };
    std::string fixture(reinterpret_cast<const char *>(bytes), sizeof(bytes));
    ReadOnlyBinaryStream input(fixture + std::string(1, char(0xab)));
    auto settings = CameraCodec::readAimAssistPreset(input);
    check(settings.mHasIdentifier && settings.mIdentifier == "audit:hitboxes", "preset identifier decoded incorrectly");
    check(settings.mHasTargetMode && settings.mTargetMode == 1, "target mode must occupy one byte");
    check(settings.mHasAngle && settings.mAngle.x == 30 && settings.mAngle.y == 30, "view angles decoded incorrectly");
    check(settings.mHasDistance && settings.mDistance == 10, "distance decoded incorrectly");
    check(input.getByte() == 0xab, "aim assist consumed the next camera field");
    BinaryStream output;
    CameraCodec::writeAimAssistPreset(output, settings);
    check(output.getBuffer() == fixture, "aim assist encoding differs from the wire fixture");
    BlockDefinitionRegistry blocks;
    ItemDefinitionRegistry items;
    PacketCodecContext context(blocks, items);
    CameraPresetsPacket packet;
    CameraPreset custom;
    custom.mIdentifier = "audit:camera";
    custom.mParentPreset = "minecraft:third_person";
    custom.mHasAimAssistPreset = true;
    custom.mAimAssistPreset = settings;
    CameraPreset next;
    next.mIdentifier = "minecraft:free";
    packet.mPresets = {custom, next};
    BinaryStream encoded;
    packet.write(encoded, context);
    ReadOnlyBinaryStream reader(encoded.getBuffer());
    CameraPresetsPacket decoded;
    decoded.read(reader, context);
    check(decoded.mPresets.size() == 2 && decoded.mPresets[1].mIdentifier == next.mIdentifier,
        "a custom aim assist preset misaligned the following preset");
}
