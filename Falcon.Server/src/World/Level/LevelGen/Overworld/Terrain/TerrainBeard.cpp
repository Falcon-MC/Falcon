#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <cstring>

namespace {

    // exp(-(dx^2 + dz^2 + (dy + 0.5)^2) / 16) on a 24^3 lattice centred on 12, built in doubles and stored
    // as floats
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

    // one entry of the game's second table: the kernel weighted by the distance to the ground,
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

    // the beard kernel on the box widened by 2, weighed by the distance to the box centre: the height is measured
    // from the ground below it, from the top above it, and is 0 between, which empties the piece
    double shaver(const TerrainBeard::Piece &piece, int32_t x, int32_t y, int32_t z) {
        const int32_t ground = piece.mMinY + piece.mGroundLevelDelta;
        int32_t nearestY = y > ground ? y : ground;
        if (y > piece.mMaxY)
            nearestY = piece.mMaxY;
        const int32_t dy = y - nearestY;
        const int32_t dx = std::max(outside(x, piece.mMinX - 2, piece.mMaxX + 2), 0);
        const int32_t dz = std::max(outside(z, piece.mMinZ - 2, piece.mMaxZ + 2), 0);
        if (dx > 11 || (uint32_t) (dy + 12) > 23u || dz > 11)
            return 0.0;

        // the reach grows by 1 at and under the ground, by 5 over it
        const int32_t sizeX = piece.mMaxX - piece.mMinX + 5;
        const int32_t sizeZ = piece.mMaxZ - piece.mMinZ + 5;
        const float radius = (y > ground ? 5.0f : 1.0f) + std::fmax((float) sizeZ * 0.5f, (float) sizeX * 0.5f);
        if (!(radius > 0.0f))
            return 0.0;

        const float awayX = (float) (x - (piece.mMinX - 2 + sizeX / 2));
        const float awayZ = (float) (z - (piece.mMinZ - 2 + sizeZ / 2));
        const float distance = std::sqrt(awayZ * awayZ + ((float) dy * (float) dy + awayX * awayX));
        if (!(distance <= radius))
            return 0.0;

        return (double) ((float) beard(dx, dz, dy, dy) * (1.5f - distance / radius));
    }

    // max(0, 1 - length / 6) on half steps, stored as a float
    double bury(double x, double y, double z) {
        const double length = std::sqrt(x * x + y * y + z * z) * 0.16666666666666666;
        return (double) (float) std::max(0.0, 1.0 - length);
    }
}

void TerrainBeard::addPiece(int32_t chunkX, int32_t chunkZ, const Piece &piece) {
    // the shaver widens the box by 2 on x and z
    const int32_t margin = piece.mAdaptation == Adaptation::BeardAndShaver ? 2 : 0;
    const int32_t minX = chunkX * 16;
    const int32_t minZ = chunkZ * 16;
    if (piece.mMaxX + margin < minX - 12 || piece.mMinX - margin > minX + 27 || piece.mMaxZ + margin < minZ - 12
        || piece.mMinZ - margin > minZ + 27)
        return;

    mPieces.push_back(piece);
    const int32_t ground = piece.mMinY + piece.mGroundLevelDelta;
    mMinY = std::min(mMinY, std::min(piece.mMinY, ground) - 12);
    mMaxY = std::max(mMaxY, std::max(piece.mMaxY, ground) + 11);
}

void TerrainBeard::addJunction(int32_t chunkX, int32_t chunkZ, int32_t x, int32_t y, int32_t z) {
    const int32_t minX = chunkX * 16;
    const int32_t minZ = chunkZ * 16;
    if (x < minX - 12 || x > minX + 27 || z < minZ - 12 || z > minZ + 27)
        return;

    mJunctions.push_back({x, y, z});
    mMinY = std::min(mMinY, y - 12);
    mMaxY = std::max(mMaxY, y + 11);
}

void TerrainBeard::addBuriedBox(int32_t chunkX, int32_t chunkZ, const Box &box) {
    const int32_t minX = chunkX * 16;
    const int32_t minZ = chunkZ * 16;
    if (box.mMaxX < minX - 12 || box.mMinX > minX + 27 || box.mMaxZ < minZ - 12 || box.mMinZ > minZ + 27)
        return;

    mBuried.push_back(box);
    mMinY = std::min(mMinY, box.mMinY - 12);
    mMaxY = std::max(mMaxY, box.mMaxY + 11);
}

float TerrainBeard::buried(int32_t x, int32_t y, int32_t z) const {
    float total = 0.0f;
    for (const Box &box: mBuried) {
        const int32_t nearestY = std::min(std::max(y, box.mMinY), box.mMaxY);
        const int32_t dx = std::max(outside(x, box.mMinX, box.mMaxX), 0);
        const int32_t dz = std::max(outside(z, box.mMinZ, box.mMaxZ), 0);
        // the height counts half, the length is truncated to a whole block before the six block falloff
        const int32_t dy = (int32_t) ((float) (y - nearestY) * 0.5f);
        const int32_t length = (int32_t) std::sqrt((double) (dy * dy + dx * dx + dz * dz));
        const float distance = (float) ((double) length / 6.0);
        if (distance <= 0.0f)
            total += 1.0f;
        else if (distance <= 1.0f)
            total += 1.0f - distance;
    }
    return total;
}

double TerrainBeard::compute(int32_t x, int32_t y, int32_t z) const {
    double total = 0.0;
    for (const Piece &piece: mPieces) {
        const int32_t outX = outside(x, piece.mMinX, piece.mMaxX);
        const int32_t outZ = outside(z, piece.mMinZ, piece.mMaxZ);
        const int32_t toGround = y - (piece.mMinY + piece.mGroundLevelDelta);

        double contribution = 0.0;
        switch (piece.mAdaptation) {
            case Adaptation::Bury:
                // no 0.8 here, and a reach of 5 instead of 11
                if (outX <= 5 && (uint32_t) (toGround + 12) <= 23u && outZ <= 5)
                    contribution = bury(std::max(outX, 0), toGround * 0.5, std::max(outZ, 0));
                break;
            case Adaptation::BeardThin:
                if (outX <= 11 && (uint32_t) (toGround + 12) <= 23u && outZ <= 11)
                    contribution = beard(std::max(outX, 0), std::max(outZ, 0), toGround, toGround) * 0.8;
                break;
            case Adaptation::Encapsulate: {
                const int32_t outY = outside(y, piece.mMinY, piece.mMaxY);
                if (outX <= 11 && outY <= 11 && outZ <= 11)
                    contribution = bury(std::max(outX, 0) * 0.5, std::max(outY, 0) * 0.5, std::max(outZ, 0) * 0.5)
                                   * 0.8;
                break;
            }
            case Adaptation::BeardAndShaver:
                contribution = shaver(piece, x, y, z);
                break;
        }
        total = total + contribution;
    }

    double around = 0.0;
    for (const Junction &junction: mJunctions) {
        const int32_t dx = x - junction.x;
        const int32_t dy = y - junction.y;
        const int32_t dz = z - junction.z;
        double contribution = 0.0;
        if ((uint32_t) (dx + 12) < 24u && (uint32_t) (dy + 12) < 24u && (uint32_t) (dz + 12) < 24u)
            contribution = beard(dx, dz, dy, dy) * 0.4;
        around = around + contribution;
    }

    return total + around;
}
