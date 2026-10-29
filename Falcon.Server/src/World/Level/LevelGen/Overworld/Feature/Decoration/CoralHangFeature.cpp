#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralHangFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"

namespace {

    using Face = DecorationSupport::FaceOffset;

    bool sameFace(const Face &left, const Face &right) {
        return left.mX == right.mX && left.mY == right.mY && left.mZ == right.mZ;
    }

    Face rotateY(const Face &face) {
        if (face.mZ == -1)
            return DecorationSupport::FACE_EAST;

        if (face.mX == 1)
            return DecorationSupport::FACE_SOUTH;

        if (face.mZ == 1)
            return DecorationSupport::FACE_WEST;

        return DecorationSupport::FACE_NORTH;
    }

    Face rotateYCounterClockwise(const Face &face) {
        if (face.mZ == -1)
            return DecorationSupport::FACE_WEST;

        if (face.mX == 1)
            return DecorationSupport::FACE_NORTH;

        if (face.mZ == 1)
            return DecorationSupport::FACE_EAST;

        return DecorationSupport::FACE_SOUTH;
    }

}

const char *CoralHangFeature::name() const {
    return "minecraft:coral_hang_feature";
}

int32_t CoralHangFeature::getBase() const {
    return 1;
}

int32_t CoralHangFeature::getRandom() const {
    return 0;
}

bool CoralHangFeature::placeFeature(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                                    const BlockState &coralState) {
    if (!placeCoralBlock(chunk, random, x, y, z, coralState))
        return false;

    const Face clawDirection = DecorationSupport::HORIZONTAL_PLANE[random.nextInt(4)];
    const int32_t branches = random.nextInt(2) + 2;
    Face possible[3] = {clawDirection, rotateY(clawDirection), rotateYCounterClockwise(clawDirection)};

    for (int32_t i = 2; i > 0; i--) {
        const int32_t j = random.nextInt(i + 1);
        const Face t = possible[i];
        possible[i] = possible[j];
        possible[j] = t;
    }

    for (int32_t branch = 0; branch < branches; branch++) {
        const Face branchDirection = possible[branch];
        int32_t px = x + branchDirection.mX;
        int32_t py = y;
        int32_t pz = z + branchDirection.mZ;

        const int32_t sidewayLength = random.nextInt(2) + 1;
        int32_t inwayLength;
        Face segmentDirection;

        if (sameFace(branchDirection, clawDirection)) {
            segmentDirection = clawDirection;
            inwayLength = random.nextInt(3) + 2;
        } else {
            py++;
            segmentDirection = random.nextBoolean() ? branchDirection : DecorationSupport::FACE_UP;
            inwayLength = random.nextInt(3) + 3;
        }

        for (int32_t i = 0; i < sidewayLength && placeCoralBlock(chunk, random, px, py, pz, coralState); i++) {
            px += segmentDirection.mX;
            py += segmentDirection.mY;
            pz += segmentDirection.mZ;
        }

        px -= segmentDirection.mX;
        py -= segmentDirection.mY;
        pz -= segmentDirection.mZ;
        py++;

        for (int32_t i = 0; i < inwayLength; i++) {
            px += clawDirection.mX;
            py += clawDirection.mY;
            pz += clawDirection.mZ;

            if (!placeCoralBlock(chunk, random, px, py, pz, coralState))
                break;

            if (random.nextFloat() < 0.25f)
                py++;
        }
    }

    return true;
}
