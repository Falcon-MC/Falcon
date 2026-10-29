#include "World/Level/LevelGen/End/Noise/TheEndImprovedNoise.h"

namespace {

    const double GRAD_X[16] = {1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0, 0.0};
    const double GRAD_Y[16] = {1.0, 1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0};
    const double GRAD_Z[16] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, -1.0, -1.0, 0.0, 1.0, 0.0, -1.0};
    const double GRAD_2X[16] = {1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0, 0.0};
    const double GRAD_2Z[16] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, -1.0, -1.0, 0.0, 1.0, 0.0, -1.0};

}

TheEndImprovedNoise::TheEndImprovedNoise(IRandom &random)
        : mPermutations(), mXCoord(0.0), mYCoord(0.0), mZCoord(0.0) {
    mPermutations.fill(0);

    mXCoord = random.nextDouble() * 256.0;
    mYCoord = random.nextDouble() * 256.0;
    mZCoord = random.nextDouble() * 256.0;

    for (int32_t i = 0; i < 256; i++)
        mPermutations[(size_t) i] = i;

    for (int32_t l = 0; l < 256; l++) {
        const int32_t j = random.nextBoundedInt(256 - l) + l;
        const int32_t k = mPermutations[(size_t) l];
        mPermutations[(size_t) l] = mPermutations[(size_t) j];
        mPermutations[(size_t) j] = k;
        mPermutations[(size_t) (l + 256)] = mPermutations[(size_t) l];
    }
}

double TheEndImprovedNoise::_lerp(double delta, double start, double end) {
    return start + delta * (end - start);
}

double TheEndImprovedNoise::_grad2(int32_t hash, double x, double z) const {
    const int32_t i = hash & 15;
    return GRAD_2X[i] * x + GRAD_2Z[i] * z;
}

double TheEndImprovedNoise::_grad(int32_t hash, double x, double y, double z) const {
    const int32_t i = hash & 15;
    return GRAD_X[i] * x + GRAD_Y[i] * y + GRAD_Z[i] * z;
}

void TheEndImprovedNoise::populateNoiseArray(std::vector<double> &noiseArray, double xOffset, double yOffset,
                                             double zOffset, int32_t xSize, int32_t ySize, int32_t zSize,
                                             double xScale, double yScale, double zScale, double noiseScale) const {
    if (ySize == 1) {
        int32_t l5 = 0;
        const double d16 = 1.0 / noiseScale;

        for (int32_t j2 = 0; j2 < xSize; ++j2) {
            double d17 = xOffset + (double) j2 * xScale + mXCoord;
            int32_t i6 = (int32_t) d17;

            if (d17 < (double) i6)
                --i6;

            const int32_t k2 = i6 & 255;
            d17 = d17 - (double) i6;
            const double d18 = d17 * d17 * d17 * (d17 * (d17 * 6.0 - 15.0) + 10.0);

            for (int32_t j6 = 0; j6 < zSize; ++j6) {
                double d19 = zOffset + (double) j6 * zScale + mZCoord;
                int32_t k6 = (int32_t) d19;

                if (d19 < (double) k6)
                    --k6;

                const int32_t l6 = k6 & 255;
                d19 = d19 - (double) k6;
                const double d20 = d19 * d19 * d19 * (d19 * (d19 * 6.0 - 15.0) + 10.0);
                const int32_t i5 = mPermutations[(size_t) k2];
                const int32_t j5 = mPermutations[(size_t) i5] + l6;
                const int32_t j = mPermutations[(size_t) (k2 + 1)];
                const int32_t k5 = mPermutations[(size_t) j] + l6;
                const double d14 = _lerp(d18, _grad2(mPermutations[(size_t) j5], d17, d19),
                                         _grad(mPermutations[(size_t) k5], d17 - 1.0, 0.0, d19));
                const double d15 = _lerp(d18, _grad(mPermutations[(size_t) (j5 + 1)], d17, 0.0, d19 - 1.0),
                                         _grad(mPermutations[(size_t) (k5 + 1)], d17 - 1.0, 0.0, d19 - 1.0));
                const double d21 = _lerp(d20, d14, d15);
                const int32_t i7 = l5++;
                noiseArray[(size_t) i7] += d21 * d16;
            }
        }

        return;
    }

    int32_t i = 0;
    const double d0 = 1.0 / noiseScale;
    int32_t k = -1;
    int32_t l = 0;
    int32_t i1 = 0;
    int32_t j1 = 0;
    int32_t k1 = 0;
    int32_t l1 = 0;
    int32_t i2 = 0;
    double d1 = 0.0;
    double d2 = 0.0;
    double d3 = 0.0;
    double d4 = 0.0;

    for (int32_t l2 = 0; l2 < xSize; ++l2) {
        double d5 = xOffset + (double) l2 * xScale + mXCoord;
        int32_t i3 = (int32_t) d5;

        if (d5 < (double) i3)
            --i3;

        const int32_t j3 = i3 & 255;
        d5 = d5 - (double) i3;
        const double d6 = d5 * d5 * d5 * (d5 * (d5 * 6.0 - 15.0) + 10.0);

        for (int32_t k3 = 0; k3 < zSize; ++k3) {
            double d7 = zOffset + (double) k3 * zScale + mZCoord;
            int32_t l3 = (int32_t) d7;

            if (d7 < (double) l3)
                --l3;

            const int32_t i4 = l3 & 255;
            d7 = d7 - (double) l3;
            const double d8 = d7 * d7 * d7 * (d7 * (d7 * 6.0 - 15.0) + 10.0);

            for (int32_t j4 = 0; j4 < ySize; ++j4) {
                double d9 = yOffset + (double) j4 * yScale + mYCoord;
                int32_t k4 = (int32_t) d9;

                if (d9 < (double) k4)
                    --k4;

                const int32_t l4 = k4 & 255;
                d9 = d9 - (double) k4;
                const double d10 = d9 * d9 * d9 * (d9 * (d9 * 6.0 - 15.0) + 10.0);

                if (j4 == 0 || l4 != k) {
                    k = l4;
                    l = mPermutations[(size_t) j3] + l4;
                    i1 = mPermutations[(size_t) l] + i4;
                    j1 = mPermutations[(size_t) (l + 1)] + i4;
                    k1 = mPermutations[(size_t) (j3 + 1)] + l4;
                    l1 = mPermutations[(size_t) k1] + i4;
                    i2 = mPermutations[(size_t) (k1 + 1)] + i4;
                    d1 = _lerp(d6, _grad(mPermutations[(size_t) i1], d5, d9, d7),
                               _grad(mPermutations[(size_t) l1], d5 - 1.0, d9, d7));
                    d2 = _lerp(d6, _grad(mPermutations[(size_t) j1], d5, d9 - 1.0, d7),
                               _grad(mPermutations[(size_t) i2], d5 - 1.0, d9 - 1.0, d7));
                    d3 = _lerp(d6, _grad(mPermutations[(size_t) (i1 + 1)], d5, d9, d7 - 1.0),
                               _grad(mPermutations[(size_t) (l1 + 1)], d5 - 1.0, d9, d7 - 1.0));
                    d4 = _lerp(d6, _grad(mPermutations[(size_t) (j1 + 1)], d5, d9 - 1.0, d7 - 1.0),
                               _grad(mPermutations[(size_t) (i2 + 1)], d5 - 1.0, d9 - 1.0, d7 - 1.0));
                }

                const double d11 = _lerp(d10, d1, d2);
                const double d12 = _lerp(d10, d3, d4);
                const double d13 = _lerp(d8, d11, d12);
                const int32_t j7 = i++;
                noiseArray[(size_t) j7] += d13 * d0;
            }
        }
    }
}
