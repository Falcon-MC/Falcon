#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/CameraTypes.h"

class CameraCodec {
public:
    static void writeEase(BinaryStream &stream, const CameraEaseData &ease);

    static CameraEaseData readEase(ReadOnlyBinaryStream &stream);

    static void writeSetInstruction(BinaryStream &stream, const CameraSetInstruction &instruction);

    static CameraSetInstruction readSetInstruction(ReadOnlyBinaryStream &stream);

    static void writeFadeInstruction(BinaryStream &stream, const CameraFadeInstruction &instruction);

    static CameraFadeInstruction readFadeInstruction(ReadOnlyBinaryStream &stream);

    static void writeSplineInstruction(BinaryStream &stream, const CameraSplineInstruction &instruction);

    static CameraSplineInstruction readSplineInstruction(ReadOnlyBinaryStream &stream);

    static void writeAimAssistPreset(BinaryStream &stream, const CameraAimAssistPreset &preset);

    static CameraAimAssistPreset readAimAssistPreset(ReadOnlyBinaryStream &stream);

    static void writePreset(BinaryStream &stream, const PacketCodecContext &context, const CameraPreset &preset);

    static CameraPreset readPreset(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);
};
