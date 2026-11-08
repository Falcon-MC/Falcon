#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <cstring>

namespace {

    // exp(-(dx^2 + dz^2 + (dy + 0.5)^2) / 16) on a 24^3 lattice centred on 12, built in doubles and stored
    // as floats (first table of sub_145C32C40)
    const std::array<float, 24 * 24 * 24> &beardKernel() {
        static const std::array<float, 24 * 24 * 24> kernel = [] {
            std::array<float, 24 * 24 * 24> table{};
            for (int32_t a = 0; a < 24; a++) {
                const double aSquared = (double) (a - 12) * (double) (a - 12);
                for (int32_t b = 0; b < 24; b++) {
                    const double bSquared = (double) (b - 12) * (double) (b - 12);
                    for (int32_t c = 0; c < 24; c++) {
                        const double cOffset = (double) c - 11.5;
                        double sum = bSquared + cOffset * cOffset;
                        sum = sum + aSquared;
                        const double exponent = sum * -0.0625;
                        table[(size_t) (a * 576 + b * 24 + c)] = (float) std::pow(2.718281828459045, exponent);
                    }
                }
            }
            return table;
        }();
        return kernel;
    }

    // one entry of the second table of sub_145C32C40: the kernel weighted by the distance to the ground,
    // with the double fast inverse square root of the game
    double beard(int32_t dx, int32_t dz, int32_t kernelY, int32_t toGround) {
        const double ground = (double) toGround + 0.5;
        const double xSquared = (double) dx * (double) dx;
        const double zSquared = (double) dz * (double) dz;
        double half = ground * ground;
        half = half + xSquared;
        half = half + zSquared;
        half = half * 0.5;

        uint64_t bits;
        std::memcpy(&bits, &half, sizeof(bits));
        bits = 0x5FE6EB50C7B537A9ull - (bits >> 1);
        double inverse;
        std::memcpy(&inverse, &bits, sizeof(inverse));

        double step = half * 0.5;
        step = step * inverse;
        step = step * inverse;
        double value = 1.5 - step;
        value = value * inverse;
        value = value * -ground;
        value = value * 0.5;
        const float kernel = beardKernel()[(size_t) ((dz + 12) * 576 + (dx + 12) * 24 + (kernelY + 12))];
        value = value * (double) kernel;
        return (double) (float) value;
    }

    int32_t outside(int32_t value, int32_t min, int32_t max) {
        return std::max(min - value, value - max);
    }
}

void TerrainBeard::addPiece(int32_t chunkX, int32_t chunkZ, const Piece &piece) {
    const int32_t minX = chunkX * 16;
    const int32_t minZ = chunkZ * 16;
    if (piece.mMaxX < minX - 12 || piece.mMinX > minX + 27 || piece.mMaxZ < minZ - 12 || piece.mMinZ > minZ + 27)
        return;

    mPieces.push_back(piece);
    const int32_t ground = piece.mMinY + piece.mGroundLevelDelta;
    mMinY = std::min(mMinY, std::min(piece.mMinY, ground) - 12);
    mMaxY = std::max(mMaxY, std::max(piece.mMaxY, ground) + 11);
}

double TerrainBeard::compute(int32_t x, int32_t y, int32_t z) const {
    double total = 0.0;
    for (const Piece &piece: mPieces) {
        const int32_t outX = outside(x, piece.mMinX, piece.mMaxX);
        const int32_t outZ = outside(z, piece.mMinZ, piece.mMaxZ);
        const int32_t toGround = y - (piece.mMinY + piece.mGroundLevelDelta);

        double contribution = 0.0;
        if (outX <= 11 && (uint32_t) (toGround + 12) <= 23u && outZ <= 11)
            contribution = beard(std::max(outX, 0), std::max(outZ, 0), toGround, toGround) * 0.8;
        total = total + contribution;
    }

    return total;
}
