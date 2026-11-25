#include "Protocol/Packets/PrimitiveShapesPacket.h"

#include <cstring>
#include <stdexcept>

static void check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

int main() {
    BlockDefinitionRegistry blocks;
    ItemDefinitionRegistry items;
    PacketCodecContext context(blocks, items);

    PrimitiveShapesPacket packet;
    DebugShapeData cylinder;
    cylinder.mId = 7;
    cylinder.mHasType = true;
    cylinder.mType = DebugShapeType::Cylinder;
    cylinder.mRadii2f = Vector2f(1.5f, 0.5f);
    cylinder.mHeight = 2.0f;
    cylinder.mSegments = 20;
    DebugShapeData removed;
    removed.mId = 8;
    packet.mShapes = {cylinder, removed};

    BinaryStream encoded;
    packet.write(encoded, context);
    const std::string &bytes = encoded.getBuffer();
    // count, id, type, eight absent optionals, variant 6, radii, height, segments; then id 8 and nine absent fields
    check(bytes.size() == 1 + 1 + 2 + 8 + 1 + 8 + 4 + 1 + 1 + 9 + 1, "cylinder carries one radii pair");
    float bottom = 0.0f;
    std::memcpy(&bottom, bytes.data() + 13, 4);
    check(bytes[12] == 6 && bottom == 1.5f, "cylinder radii misplaced");

    ReadOnlyBinaryStream reader(bytes);
    PrimitiveShapesPacket decoded;
    decoded.read(reader, context);
    check(decoded.mShapes.size() == 2, "shape count decoded incorrectly");
    check(decoded.mShapes[0].mRadii2f.y == 0.5f && decoded.mShapes[0].mHeight == 2.0f
                  && decoded.mShapes[0].mSegments == 20,
          "cylinder decoded incorrectly");
    check(decoded.mShapes[1].mId == 8 && !decoded.mShapes[1].mHasType, "a removal lost its id");
}
