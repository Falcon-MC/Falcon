#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"

const char *CoralTreeFeature::name() const {
    return "minecraft:coral_tree_feature";
}

int32_t CoralTreeFeature::getBase() const {
    return 1;
}

int32_t CoralTreeFeature::getRandom() const {
    return 0;
}

bool CoralTreeFeature::placeFeature(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                                    const BlockState &coralState) {
    int32_t py = y;
    const int32_t trunkHeight = random.nextInt(3) + 1;

    for (int32_t i = 0; i < trunkHeight; i++) {
        if (!placeCoralBlock(chunk, random, x, py, z, coralState))
            return true;

        py++;
    }

    const int32_t topY = py;
    const int32_t branches = random.nextInt(3) + 2;

    DecorationSupport::FaceOffset directions[4] = {
            DecorationSupport::HORIZONTALS[0],
            DecorationSupport::HORIZONTALS[1],
            DecorationSupport::HORIZONTALS[2],
            DecorationSupport::HORIZONTALS[3]
    };

    for (int32_t i = 3; i > 0; i--) {
        const int32_t j = random.nextInt(i + 1);
        const DecorationSupport::FaceOffset t = directions[i];
        directions[i] = directions[j];
        directions[j] = t;
    }

    for (int32_t i = 0; i < branches; i++) {
        const DecorationSupport::FaceOffset direction = directions[i];
        int32_t px = x + direction.mX;
        int32_t pz = z + direction.mZ;
        const int32_t branchHeight = random.nextInt(5) + 2;
        int32_t segmentLength = 0;
        int32_t by = topY;

        for (int32_t j = 0; j < branchHeight && placeCoralBlock(chunk, random, px, by, pz, coralState); j++) {
            segmentLength++;
            by++;
            if (j == 0 || (segmentLength >= 2 && random.nextFloat() < 0.25f)) {
                px += direction.mX;
                pz += direction.mZ;
                segmentLength = 0;
            }
        }
    }

    return true;
}
