#include "World/Level/LevelGen/Overworld/Feature/Decoration/CoralCrustFeature.h"

const char *CoralCrustFeature::name() const {
    return "minecraft:coral_crust_feature";
}

int32_t CoralCrustFeature::getBase() const {
    return 1;
}

int32_t CoralCrustFeature::getRandom() const {
    return 0;
}

bool CoralCrustFeature::placeFeature(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                                     const BlockState &coralState) {
    const int32_t height = random.nextInt(3) + 3;
    const int32_t width = random.nextInt(3) + 3;
    const int32_t length = random.nextInt(3) + 3;
    const int32_t sink = random.nextInt(3) + 1;

    for (int32_t ix = 0; ix <= width; ix++) {
        for (int32_t iy = 0; iy <= height; iy++) {
            for (int32_t iz = 0; iz <= length; iz++) {
                const int32_t px = x + ix;
                const int32_t py = y + iy - sink;
                const int32_t pz = z + iz;

                if ((((ix != 0 && ix != width) || (iy != 0 && iy != height))
                     && ((iz != 0 && iz != length) || (iy != 0 && iy != height))
                     && ((ix != 0 && ix != width) || (iz != 0 && iz != length))
                     && (ix == 0 || ix == width || iy == 0 || iy == height || iz == 0 || iz == length))
                    && random.nextFloat() >= 0.1f)
                    placeCoralBlock(chunk, random, px, py, pz, coralState);
            }
        }
    }

    return true;
}
