#include "Protocol/SkinCodec.h"

#include "Core/Utility/BinaryDataException.h"

#include <string>

void SkinCodec::writeImage(BinaryStream &stream, const SkinImageData &image) {
    stream.putLInt((uint32_t) image.mWidth);
    stream.putLInt((uint32_t) image.mHeight);
    stream.putByteArray(image.mData);
}

SkinImageData SkinCodec::readImage(ReadOnlyBinaryStream &stream) {
    SkinImageData image;
    image.mWidth = (int32_t) stream.getLInt();
    image.mHeight = (int32_t) stream.getLInt();
    image.mData = stream.getByteArray();
    return image;
}

void SkinCodec::writeAnimation(BinaryStream &stream, const SkinAnimationData &animation) {
    writeImage(stream, animation.mImage);
    stream.putUnsignedVarInt((uint32_t) animation.mTextureType);
    stream.putLFloat(animation.mFrames);
    stream.putUnsignedVarInt((uint32_t) animation.mExpressionType);
}

SkinAnimationData SkinCodec::readAnimation(ReadOnlyBinaryStream &stream) {
    SkinAnimationData animation;
    animation.mImage = readImage(stream);
    animation.mTextureType = (int32_t) stream.getUnsignedVarInt();
    animation.mFrames = stream.getLFloat();
    animation.mExpressionType = (int32_t) stream.getUnsignedVarInt();
    return animation;
}

void SkinCodec::writeSkin(BinaryStream &stream, const SerializedSkin &skin, const std::string *playFabId) {
    stream.putString(skin.mSkinId);
    if (playFabId != nullptr)
        stream.putString(*playFabId);
    stream.putString(skin.mSkinResourcePatch);
    writeImage(stream, skin.mSkinData);

    stream.putUnsignedVarInt((uint32_t) skin.mAnimations.size());
    for (const SkinAnimationData &animation: skin.mAnimations)
        writeAnimation(stream, animation);

    writeImage(stream, skin.mCapeData);
    stream.putString(skin.mGeometryData);
    stream.putString(skin.mGeometryDataEngineVersion);
    stream.putString(skin.mAnimationData);
    stream.putString(skin.mCapeId);
    stream.putString(skin.mFullSkinId);

    stream.putByte(skin.mArmsWide ? 1 : 0);
    stream.putLInt((uint32_t) skin.mColorArgb);

    stream.putUnsignedVarInt((uint32_t) skin.mPersonaPieces.size());
    for (const PersonaPieceData &piece: skin.mPersonaPieces) {
        stream.putString(piece.mId);
        stream.putLInt((uint32_t) piece.mPieceType);
        stream.putUuid(piece.mPackId);
        stream.putBool(piece.mIsDefault);
        stream.putString(piece.mProductId);
    }

    stream.putUnsignedVarInt((uint32_t) skin.mTintColors.size());
    for (const PersonaPieceTintData &tint: skin.mTintColors) {
        stream.putString(tint.mType);
        for (int i = 0; i < 4; i++) {
            const uint32_t color = i < (int) tint.mColors.size() ? (uint32_t) tint.mColors[i] : 0u;
            stream.putLInt(color);
        }
    }

    stream.putBool(skin.mPremium);
    stream.putBool(skin.mPersona);
    stream.putBool(skin.mCapeOnClassic);
    stream.putBool(skin.mPrimaryUser);
    stream.putBool(skin.mOverridingPlayerAppearance);

    stream.putString(skin.mTrusted ? "true" : "false");
    stream.putString(skin.mProfileHash);
}

SerializedSkin SkinCodec::readSkin(ReadOnlyBinaryStream &stream, std::string *outPlayFabId) {
    SerializedSkin skin;
    skin.mSkinId = stream.getString();
    if (outPlayFabId != nullptr)
        *outPlayFabId = stream.getString();
    skin.mSkinResourcePatch = stream.getString();
    skin.mSkinData = readImage(stream);

    // counts are held to what the packet can encode, by the smallest entry: image of two ints and an
    // empty array (9), type, frames (4), expression
    const uint32_t animationCount = stream.getUnsignedVarInt();
    if (animationCount > stream.getRemainingLength() / 15)
        throw BinaryDataException("Skin animation count " + std::to_string(animationCount) + " exceeds the packet");
    skin.mAnimations.reserve(animationCount);
    for (uint32_t i = 0; i < animationCount; i++)
        skin.mAnimations.push_back(readAnimation(stream));

    skin.mCapeData = readImage(stream);
    skin.mGeometryData = stream.getString();
    skin.mGeometryDataEngineVersion = stream.getString();
    skin.mAnimationData = stream.getString();
    skin.mCapeId = stream.getString();
    skin.mFullSkinId = stream.getString();

    skin.mArmsWide = stream.getByte() != 0;
    skin.mColorArgb = (int32_t) stream.getLInt();

    const uint32_t pieceCount = stream.getUnsignedVarInt();
    // two empty strings, a type (4), a pack id (16), a flag
    if (pieceCount > stream.getRemainingLength() / 23)
        throw BinaryDataException("Persona piece count " + std::to_string(pieceCount) + " exceeds the packet");
    skin.mPersonaPieces.reserve(pieceCount);
    for (uint32_t i = 0; i < pieceCount; i++) {
        PersonaPieceData piece;
        piece.mId = stream.getString();
        piece.mPieceType = (int32_t) stream.getLInt();
        piece.mPackId = stream.getUuid();
        piece.mIsDefault = stream.getBool();
        piece.mProductId = stream.getString();
        skin.mPersonaPieces.push_back(piece);
    }

    const uint32_t tintCount = stream.getUnsignedVarInt();
    // an empty string and four colours
    if (tintCount > stream.getRemainingLength() / 17)
        throw BinaryDataException("Persona tint count " + std::to_string(tintCount) + " exceeds the packet");
    skin.mTintColors.reserve(tintCount);
    for (uint32_t i = 0; i < tintCount; i++) {
        PersonaPieceTintData tint;
        tint.mType = stream.getString();
        tint.mColors.reserve(4);
        for (int j = 0; j < 4; j++)
            tint.mColors.push_back((int32_t) stream.getLInt());
        skin.mTintColors.push_back(tint);
    }

    skin.mPremium = stream.getBool();
    skin.mPersona = stream.getBool();
    skin.mCapeOnClassic = stream.getBool();
    skin.mPrimaryUser = stream.getBool();
    skin.mOverridingPlayerAppearance = stream.getBool();

    skin.mTrusted = stream.getString() == "true";
    skin.mProfileHash = stream.getString();

    return skin;
}
