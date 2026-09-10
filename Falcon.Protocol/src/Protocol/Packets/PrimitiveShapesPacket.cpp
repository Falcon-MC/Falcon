#include "Protocol/Packets/PrimitiveShapesPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    int32_t toPayloadType(bool hasType, DebugShapeType type) {
        if (!hasType)
            return 0;

        switch (type) {
            case DebugShapeType::Arrow:
                return 1;
            case DebugShapeType::Text:
                return 2;
            case DebugShapeType::Box:
                return 3;
            case DebugShapeType::Line:
                return 4;
            case DebugShapeType::Sphere:
            case DebugShapeType::Circle:
                return 5;
            case DebugShapeType::Cylinder:
                return 6;
            case DebugShapeType::Pyramid:
                return 7;
            case DebugShapeType::Ellipsoid:
                return 8;
            case DebugShapeType::Cone:
                return 9;
            default:
                return 0;
        }
    }

    void writeShape(BinaryStream &stream, const DebugShapeData &shape) {
        stream.putUnsignedVarLong(shape.mId);

        stream.putOptionalPresent(shape.mHasType);
        if (shape.mHasType)
            stream.putByte((unsigned char) shape.mType);

        stream.putOptionalPresent(shape.mHasPosition);
        if (shape.mHasPosition)
            stream.putVector3f(shape.mPosition);

        stream.putOptionalPresent(shape.mHasScale);
        if (shape.mHasScale)
            stream.putLFloat(shape.mScale);

        stream.putOptionalPresent(shape.mHasRotation);
        if (shape.mHasRotation)
            stream.putVector3f(shape.mRotation);

        stream.putOptionalPresent(shape.mHasTotalTimeLeft);
        if (shape.mHasTotalTimeLeft)
            stream.putLFloat(shape.mTotalTimeLeft);

        stream.putOptionalPresent(shape.mHasMaximumRenderDistance);
        if (shape.mHasMaximumRenderDistance)
            stream.putLFloat(shape.mMaximumRenderDistance);

        stream.putOptionalPresent(shape.mHasColor);
        if (shape.mHasColor)
            stream.putLInt(shape.mColor);

        stream.putOptionalPresent(shape.mHasDimension);
        if (shape.mHasDimension)
            stream.putVarInt(shape.mDimension);

    stream.putOptionalPresent(shape.mHasAttachedToActorId);
    if (shape.mHasAttachedToActorId)
        stream.putUnsignedVarLong(shape.mAttachedToActorId);

        stream.putUnsignedVarInt((uint32_t) toPayloadType(shape.mHasType, shape.mType));

        if (!shape.mHasType)
            return;

        switch (shape.mType) {
            case DebugShapeType::Arrow:
                stream.putOptionalPresent(shape.mHasArrowEndPosition);
                if (shape.mHasArrowEndPosition)
                    stream.putVector3f(shape.mArrowEndPosition);
                stream.putOptionalPresent(shape.mHasArrowHeadLength);
                if (shape.mHasArrowHeadLength)
                    stream.putLFloat(shape.mArrowHeadLength);
                stream.putOptionalPresent(shape.mHasArrowHeadRadius);
                if (shape.mHasArrowHeadRadius)
                    stream.putLFloat(shape.mArrowHeadRadius);
                stream.putOptionalPresent(shape.mHasArrowHeadSegments);
                if (shape.mHasArrowHeadSegments)
                    stream.putByte((unsigned char) shape.mArrowHeadSegments);
                break;
            case DebugShapeType::Box:
                stream.putVector3f(shape.mBoxBounds);
                break;
            case DebugShapeType::Circle:
                stream.putByte((unsigned char) shape.mSegments);
                break;
            case DebugShapeType::Line:
                stream.putVector3f(shape.mLineEndPosition);
                break;
            case DebugShapeType::Sphere:
                stream.putByte((unsigned char) shape.mSegments);
                break;
            case DebugShapeType::Text:
                stream.putString(shape.mText);
                stream.putBool(shape.mUseRotation);
                stream.putOptionalPresent(shape.mHasBackgroundColor);
                if (shape.mHasBackgroundColor)
                    stream.putLInt(shape.mBackgroundColor);
                stream.putLFloat(shape.mLineGapHeight);
                stream.putBool(shape.mDepthTest);
                stream.putBool(shape.mShowBackface);
                stream.putBool(shape.mShowTextBackface);
                break;
            case DebugShapeType::Cylinder:
                stream.putVector2f(shape.mRadiusX);
                stream.putVector2f(shape.mRadiusZ);
                stream.putLFloat(shape.mHeight);
                stream.putByte((unsigned char) shape.mSegments);
                break;
            case DebugShapeType::Pyramid:
                stream.putLFloat(shape.mWidth);
                stream.putOptionalPresent(shape.mHasDepth);
                if (shape.mHasDepth)
                    stream.putLFloat(shape.mDepth);
                stream.putLFloat(shape.mHeight);
                break;
            case DebugShapeType::Ellipsoid:
                stream.putVector3f(shape.mRadii3f);
                stream.putByte((unsigned char) shape.mSegments);
                break;
            case DebugShapeType::Cone:
                stream.putVector2f(shape.mRadii2f);
                stream.putLFloat(shape.mHeight);
                stream.putByte((unsigned char) shape.mSegments);
                break;
        }
    }

    DebugShapeData readShape(ReadOnlyBinaryStream &stream) {
        DebugShapeData shape;
        shape.mId = stream.getUnsignedVarLong();

        shape.mHasType = stream.getOptionalPresent();
        if (shape.mHasType)
            shape.mType = (DebugShapeType) stream.getByte();

        shape.mHasPosition = stream.getOptionalPresent();
        if (shape.mHasPosition)
            shape.mPosition = stream.getVector3f();

        shape.mHasScale = stream.getOptionalPresent();
        if (shape.mHasScale)
            shape.mScale = stream.getLFloat();

        shape.mHasRotation = stream.getOptionalPresent();
        if (shape.mHasRotation)
            shape.mRotation = stream.getVector3f();

        shape.mHasTotalTimeLeft = stream.getOptionalPresent();
        if (shape.mHasTotalTimeLeft)
            shape.mTotalTimeLeft = stream.getLFloat();

        shape.mHasMaximumRenderDistance = stream.getOptionalPresent();
        if (shape.mHasMaximumRenderDistance)
            shape.mMaximumRenderDistance = stream.getLFloat();

        shape.mHasColor = stream.getOptionalPresent();
        if (shape.mHasColor)
            shape.mColor = stream.getLInt();

        shape.mHasDimension = stream.getOptionalPresent();
        if (shape.mHasDimension)
            shape.mDimension = stream.getVarInt();

    shape.mHasAttachedToActorId = stream.getOptionalPresent();
    if (shape.mHasAttachedToActorId)
        shape.mAttachedToActorId = stream.getUnsignedVarLong();

        stream.getUnsignedVarInt();

        if (!shape.mHasType)
            return shape;

        switch (shape.mType) {
            case DebugShapeType::Arrow:
                shape.mHasArrowEndPosition = stream.getOptionalPresent();
                if (shape.mHasArrowEndPosition)
                    shape.mArrowEndPosition = stream.getVector3f();
                shape.mHasArrowHeadLength = stream.getOptionalPresent();
                if (shape.mHasArrowHeadLength)
                    shape.mArrowHeadLength = stream.getLFloat();
                shape.mHasArrowHeadRadius = stream.getOptionalPresent();
                if (shape.mHasArrowHeadRadius)
                    shape.mArrowHeadRadius = stream.getLFloat();
                shape.mHasArrowHeadSegments = stream.getOptionalPresent();
                if (shape.mHasArrowHeadSegments)
                    shape.mArrowHeadSegments = stream.getByte();
                break;
            case DebugShapeType::Box:
                shape.mBoxBounds = stream.getVector3f();
                break;
            case DebugShapeType::Circle:
                shape.mSegments = stream.getByte();
                break;
            case DebugShapeType::Line:
                shape.mLineEndPosition = stream.getVector3f();
                break;
            case DebugShapeType::Sphere:
                shape.mSegments = stream.getByte();
                break;
            case DebugShapeType::Text:
                shape.mText = stream.getString();
                shape.mUseRotation = stream.getBool();
                shape.mHasBackgroundColor = stream.getOptionalPresent();
                if (shape.mHasBackgroundColor)
                    shape.mBackgroundColor = stream.getLInt();
                shape.mLineGapHeight = stream.getLFloat();
                shape.mDepthTest = stream.getBool();
                shape.mShowBackface = stream.getBool();
                shape.mShowTextBackface = stream.getBool();
                break;
            case DebugShapeType::Cylinder:
                shape.mRadiusX = stream.getVector2f();
                shape.mRadiusZ = stream.getVector2f();
                shape.mHeight = stream.getLFloat();
                shape.mSegments = stream.getByte();
                break;
            case DebugShapeType::Pyramid:
                shape.mWidth = stream.getLFloat();
                shape.mHasDepth = stream.getOptionalPresent();
                if (shape.mHasDepth)
                    shape.mDepth = stream.getLFloat();
                shape.mHeight = stream.getLFloat();
                break;
            case DebugShapeType::Ellipsoid:
                shape.mRadii3f = stream.getVector3f();
                shape.mSegments = stream.getByte();
                break;
            case DebugShapeType::Cone:
                shape.mRadii2f = stream.getVector2f();
                shape.mHeight = stream.getLFloat();
                shape.mSegments = stream.getByte();
                break;
        }

        return shape;
    }

}

PrimitiveShapesPacket::PrimitiveShapesPacket() = default;

void PrimitiveShapesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mShapes.size());
    for (const DebugShapeData &shape : mShapes) {
        writeShape(stream, shape);
    }
}

void PrimitiveShapesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t count = stream.getArrayLength();
    mShapes.clear();
    mShapes.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mShapes.push_back(readShape(stream));
    }
}

void PrimitiveShapesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
