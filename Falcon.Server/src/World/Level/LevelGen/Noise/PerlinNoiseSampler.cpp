#include "World/Level/LevelGen/Noise/PerlinNoiseSampler.h"

#include <cmath>

namespace {

const double FLAT_SIMPLEX_GRAD[64] = {
    1, 1, 0, 0,
    -1, 1, 0, 0,
    1, -1, 0, 0,
    -1, -1, 0, 0,
    1, 0, 1, 0,
    -1, 0, 1, 0,
    1, 0, -1, 0,
    -1, 0, -1, 0,
    0, 1, 1, 0,
    0, -1, 1, 0,
    0, 1, -1, 0,
    0, -1, -1, 0,
    1, 1, 0, 0,
    0, -1, 1, 0,
    -1, 1, 0, 0,
    0, -1, -1, 0
};

}

PerlinNoiseSampler::PerlinNoiseSampler(IRandom &rand)
    : Noise(rand) {
}

double PerlinNoiseSampler::sample(double x, double y, double z, double yAmplification, double minY) const {
    const double offsetX = x + mOriginX;
    const double offsetY = y + mOriginY;
    const double offsetZ = z + mOriginZ;
    const double floorX = std::floor(offsetX);
    const double floorY = std::floor(offsetY);
    const double floorZ = std::floor(offsetZ);
    const double localX = offsetX - floorX;
    const double localY = offsetY - floorY;
    const double localZ = offsetZ - floorZ;
    double yOffset = 0.0;

    if (yAmplification != 0.0) {
        const double yClamp = (minY >= 0.0 && minY < localY) ? minY : localY;
        yOffset = std::floor(yClamp / yAmplification + 1.0E-7) * yAmplification;
    }

    return _sample(static_cast<int32_t>(floorX), static_cast<int32_t>(floorY), static_cast<int32_t>(floorZ), localX, localY - yOffset, localZ, localY);
}

double PerlinNoiseSampler::_sample(int32_t sectionX, int32_t sectionY, int32_t sectionZ, double localX, double localY, double localZ, double fadeLocalY) const {
    const int32_t var0 = sectionX & 0xFF;
    const int32_t var1 = (sectionX + 1) & 0xFF;
    const int32_t var2 = static_cast<int32_t>(mPermutations[static_cast<size_t>(var0)]) & 0xFF;
    const int32_t var3 = static_cast<int32_t>(mPermutations[static_cast<size_t>(var1)]) & 0xFF;
    const int32_t var4 = (var2 + sectionY) & 0xFF;
    const int32_t var5 = (var3 + sectionY) & 0xFF;
    const int32_t var6 = (var2 + sectionY + 1) & 0xFF;
    const int32_t var7 = (var3 + sectionY + 1) & 0xFF;
    const int32_t var8 = static_cast<int32_t>(mPermutations[static_cast<size_t>(var4)]) & 0xFF;
    const int32_t var9 = static_cast<int32_t>(mPermutations[static_cast<size_t>(var5)]) & 0xFF;
    const int32_t var10 = static_cast<int32_t>(mPermutations[static_cast<size_t>(var6)]) & 0xFF;
    const int32_t var11 = static_cast<int32_t>(mPermutations[static_cast<size_t>(var7)]) & 0xFF;

    const int32_t var12 = (var8 + sectionZ) & 0xFF;
    const int32_t var13 = (var9 + sectionZ) & 0xFF;
    const int32_t var14 = (var10 + sectionZ) & 0xFF;
    const int32_t var15 = (var11 + sectionZ) & 0xFF;
    const int32_t var16 = (var8 + sectionZ + 1) & 0xFF;
    const int32_t var17 = (var9 + sectionZ + 1) & 0xFF;
    const int32_t var18 = (var10 + sectionZ + 1) & 0xFF;
    const int32_t var19 = (var11 + sectionZ + 1) & 0xFF;
    const int32_t var20 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var12)]) & 15) << 2;
    const int32_t var21 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var13)]) & 15) << 2;
    const int32_t var22 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var14)]) & 15) << 2;
    const int32_t var23 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var15)]) & 15) << 2;
    const int32_t var24 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var16)]) & 15) << 2;
    const int32_t var25 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var17)]) & 15) << 2;
    const int32_t var26 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var18)]) & 15) << 2;
    const int32_t var27 = (static_cast<int32_t>(mPermutations[static_cast<size_t>(var19)]) & 15) << 2;

    const double xMinusOne = localX - 1.0;
    const double yMinusOne = localY - 1.0;
    const double zMinusOne = localZ - 1.0;

    const double grad000 = FLAT_SIMPLEX_GRAD[var20] * localX + FLAT_SIMPLEX_GRAD[var20 | 1] * localY + FLAT_SIMPLEX_GRAD[var20 | 2] * localZ;
    const double grad100 = FLAT_SIMPLEX_GRAD[var21] * xMinusOne + FLAT_SIMPLEX_GRAD[var21 | 1] * localY + FLAT_SIMPLEX_GRAD[var21 | 2] * localZ;
    const double grad010 = FLAT_SIMPLEX_GRAD[var22] * localX + FLAT_SIMPLEX_GRAD[var22 | 1] * yMinusOne + FLAT_SIMPLEX_GRAD[var22 | 2] * localZ;
    const double grad110 = FLAT_SIMPLEX_GRAD[var23] * xMinusOne + FLAT_SIMPLEX_GRAD[var23 | 1] * yMinusOne + FLAT_SIMPLEX_GRAD[var23 | 2] * localZ;
    const double grad001 = FLAT_SIMPLEX_GRAD[var24] * localX + FLAT_SIMPLEX_GRAD[var24 | 1] * localY + FLAT_SIMPLEX_GRAD[var24 | 2] * zMinusOne;
    const double grad101 = FLAT_SIMPLEX_GRAD[var25] * xMinusOne + FLAT_SIMPLEX_GRAD[var25 | 1] * localY + FLAT_SIMPLEX_GRAD[var25 | 2] * zMinusOne;
    const double grad011 = FLAT_SIMPLEX_GRAD[var26] * localX + FLAT_SIMPLEX_GRAD[var26 | 1] * yMinusOne + FLAT_SIMPLEX_GRAD[var26 | 2] * zMinusOne;
    const double grad111 = FLAT_SIMPLEX_GRAD[var27] * xMinusOne + FLAT_SIMPLEX_GRAD[var27 | 1] * yMinusOne + FLAT_SIMPLEX_GRAD[var27 | 2] * zMinusOne;

    const double fadeX = localX * localX * localX * (localX * (localX * 6.0 - 15.0) + 10.0);
    const double fadeY = fadeLocalY * fadeLocalY * fadeLocalY * (fadeLocalY * (fadeLocalY * 6.0 - 15.0) + 10.0);
    const double fadeZ = localZ * localZ * localZ * (localZ * (localZ * 6.0 - 15.0) + 10.0);

    const double x00 = grad000 + fadeX * (grad100 - grad000);
    const double x10 = grad010 + fadeX * (grad110 - grad010);
    const double x01 = grad001 + fadeX * (grad101 - grad001);
    const double x11 = grad011 + fadeX * (grad111 - grad011);
    const double y0 = x00 + fadeY * (x10 - x00);
    const double y1 = x01 + fadeY * (x11 - x01);

    return y0 + fadeZ * (y1 - y0);
}
