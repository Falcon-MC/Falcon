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

    // max(0, 1 - |(x, y, z)| / 6) in halves of a block (sub_145C33490)
    double bury(int32_t halfX, int32_t halfY, int32_t halfZ) {
        const double x = (double) halfX * 0.5;
        const double y = (double) halfY * 0.5;
        const double z = (double) halfZ * 0.5;
        double sum = x * x + y * y;
        sum = sum + z * z;
        double distance = std::sqrt(sum);
        distance = distance * 0.16666666666666666;
        return (double) (float) (distance <= 1.0 ? 1.0 - distance : 0.0);
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
    if (piece.mAdaptation == Adaptation::None || (uint8_t) piece.mAdaptation > 4)
        return;

    const int32_t ground = piece.mMinY + piece.mGroundLevelDelta;
    mMinY = std::min(mMinY, std::min(piece.mMinY, ground) - 12);
    mMaxY = std::max(mMaxY, std::max(piece.mMaxY, ground) + 11);
}

void TerrainBeard::addJunction(int32_t chunkX, int32_t chunkZ, const Junction &junction) {
    const int32_t minX = chunkX * 16;
    const int32_t minZ = chunkZ * 16;
    if (junction.mX < minX || junction.mX > minX + 27 || junction.mZ < minZ || junction.mZ > minZ + 27)
        return;

    mJunctions.push_back(junction);
    mMinY = std::min(mMinY, junction.mY - 12);
    mMaxY = std::max(mMaxY, junction.mY + 11);
}

double TerrainBeard::compute(int32_t x, int32_t y, int32_t z) const {
    double total = 0.0;
    for (const Piece &piece: mPieces) {
        const int32_t outX = outside(x, piece.mMinX, piece.mMaxX);
        const int32_t outZ = outside(z, piece.mMinZ, piece.mMaxZ);
        const int32_t dx = std::max(outX, 0);
        const int32_t dz = std::max(outZ, 0);
        const int32_t ground = piece.mMinY + piece.mGroundLevelDelta;
        const int32_t toGround = y - ground;

        double contribution;
        switch (piece.mAdaptation) {
            case Adaptation::Bury:
                contribution = 0.0;
                if (outX <= 5 && (uint32_t) (toGround + 12) <= 23u && outZ <= 5)
                    contribution = bury(2 * dx, toGround, 2 * dz);
                break;
            case Adaptation::BeardThin:
                contribution = 0.0;
                if (outX <= 11 && (uint32_t) (toGround + 12) <= 23u && outZ <= 11)
                    contribution = beard(dx, dz, toGround, toGround) * 0.8;
                break;
            case Adaptation::BeardBox: {
                const int32_t outY = std::max(ground - y, y - piece.mMaxY);
                contribution = 0.0;
                if (outX <= 11 && outY <= 11 && outZ <= 11)
                    contribution = beard(dx, dz, std::max(outY, 0), toGround) * 0.8;
                break;
            }
            case Adaptation::Encapsulate: {
                const int32_t outY = std::max(piece.mMinY - y, y - piece.mMaxY);
                contribution = 0.0;
                if (outX <= 11 && outY <= 11 && outZ <= 11)
                    contribution = bury(dx, std::max(outY, 0), dz) * 0.8;
                break;
            }
            default:
                continue;
        }
        total = total + contribution;
    }

    double junctions = 0.0;
    for (const Junction &junction: mJunctions) {
        const int32_t dx = x - junction.mX;
        const int32_t dy = y - junction.mY;
        const int32_t dz = z - junction.mZ;
        double contribution = 0.0;
        if ((uint32_t) (dx + 12) < 24u && (uint32_t) (dy + 12) < 24u && (uint32_t) (dz + 12) < 24u)
            contribution = beard(dx, dz, dy, dy) * 0.4;
        junctions = junctions + contribution;
    }

    return total + junctions;
}
