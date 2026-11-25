#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/Types/SerializedSkin.h"

class SkinCodec {
public:
    /**
     * Older versions carry the PlayFab ID inside the skin, right after the skin ID; pass it for them.
     */
    static void writeSkin(BinaryStream &stream, const SerializedSkin &skin, const std::string *playFabId = nullptr);

    static SerializedSkin readSkin(ReadOnlyBinaryStream &stream, std::string *outPlayFabId = nullptr);

private:
    static void writeImage(BinaryStream &stream, const SkinImageData &image);

    static SkinImageData readImage(ReadOnlyBinaryStream &stream);

    static void writeAnimation(BinaryStream &stream, const SkinAnimationData &animation);

    static SkinAnimationData readAnimation(ReadOnlyBinaryStream &stream);
};
