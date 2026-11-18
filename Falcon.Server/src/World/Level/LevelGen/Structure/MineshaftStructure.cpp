#include "World/Level/LevelGen/Structure/MineshaftStructure.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Structure/MineshaftFeature.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace {
    using namespace MineshaftStructure;
    using Box = StructurePiece::Box;
    constexpr int32_t SOUTH = StructurePiece::SOUTH;
    constexpr int32_t WEST = StructurePiece::WEST;
    constexpr int32_t NORTH = StructurePiece::NORTH;
    constexpr int32_t EAST = StructurePiece::EAST;

    class Planner {
    public:
        Planner(std::mt19937 &random, bool mesa, int32_t x, int32_t z) : mRandom(random) {
            const int32_t length = draw(6);
            const int32_t height = draw(6);
            const int32_t width = draw(6);
            const Box room{x + 2, 50, z + 2, x + 9 + width, 54 + height, z + 9 + length};
            mPieces.emplace_back(Type::Room, room, StructurePiece::NO_ORIENTATION, 0);

            if (GameRandom::nextFloat(mRandom) > (mesa ? 0.5f : 1.0f))
                return;

            const int32_t doorHeight = std::max(1, room.mMaxY - room.mMinY - 3);
            for (const int32_t direction: {NORTH, SOUTH, WEST, EAST}) {
                const bool alongX = (direction & 1) == 0;
                const int32_t span = alongX ? room.mMaxX - room.mMinX + 1 : room.mMaxZ - room.mMinZ + 1;
                for (int32_t offset = 0; offset < span; offset += 4) {
                    offset += draw(span);
                    if (offset + 3 > span)
                        break;
                    const int32_t y = room.mMinY + draw(doorHeight) + 1;
                    std::optional<Box> child;
                    switch (direction) {
                        case NORTH: child = branch(room.mMinX + offset, y, room.mMinZ - 1, NORTH, 0); break;
                        case SOUTH: child = branch(room.mMinX + offset, y, room.mMaxZ + 1, SOUTH, 0); break;
                        case WEST: child = branch(room.mMinX - 1, y, room.mMinZ + offset, WEST, 0); break;
                        case EAST: child = branch(room.mMaxX + 1, y, room.mMinZ + offset, EAST, 0); break;
                    }
                    if (!child)
                        continue;
                    switch (direction) {
                        case NORTH: child->mMinZ = room.mMinZ; child->mMaxZ = room.mMinZ + 1; break;
                        case SOUTH: child->mMinZ = room.mMaxZ - 1; child->mMaxZ = room.mMaxZ; break;
                        case WEST: child->mMinX = room.mMinX; child->mMaxX = room.mMinX + 1; break;
                        case EAST: child->mMinX = room.mMaxX - 1; child->mMaxX = room.mMaxX; break;
                    }
                    mPieces.front().mDoorways.push_back(*child);
                }
            }
        }

        Plan finish(const ChunkGenerator &generator, bool mesa) {
            Box bounds = mPieces.front().getBox();
            for (const Piece &piece: mPieces) {
                const Box &box = piece.getBox();
                bounds.mMinX = std::min(bounds.mMinX, box.mMinX);
                bounds.mMinY = std::min(bounds.mMinY, box.mMinY);
                bounds.mMinZ = std::min(bounds.mMinZ, box.mMinZ);
                bounds.mMaxX = std::max(bounds.mMaxX, box.mMaxX);
                bounds.mMaxY = std::max(bounds.mMaxY, box.mMaxY);
                bounds.mMaxZ = std::max(bounds.mMaxZ, box.mMaxZ);
            }

            const int32_t sea = OverworldGeneratorConstants::SEA_LEVEL;
            const int32_t height = bounds.mMaxY - bounds.mMinY + 1;
            int32_t shift;
            if (mesa) {
                const int32_t centerX = bounds.mMinX + (bounds.mMaxX - bounds.mMinX + 1) / 2;
                const int32_t centerZ = bounds.mMinZ + (bounds.mMaxZ - bounds.mMinZ + 1) / 2;
                const int32_t surface = generator.getPreliminarySurfaceLevel(centerX >> 2, centerZ >> 2);
                const int32_t target = surface > sea ? sea + draw(surface - sea + 1) : sea;
                shift = target - bounds.mMinY - height / 2;
            } else {
                int32_t top = OverworldGeneratorConstants::MIN_Y + height + 1;
                if (top < sea - 10)
                    top += draw(sea - 10 - top);
                shift = top - bounds.mMaxY;
            }

            bounds.move(0, shift, 0);
            for (Piece &piece: mPieces) {
                piece.getBox().move(0, shift, 0);
                for (Box &door: piece.mDoorways)
                    door.move(0, shift, 0);
            }
            return {bounds, mesa, std::move(mPieces)};
        }

    private:
        int32_t draw(int32_t bound) {
            return GameRandom::nextInt(mRandom, bound);
        }

        bool collides(const Box &box) const {
            return std::any_of(mPieces.begin(), mPieces.end(), [&box](const Piece &piece) {
                return box.intersects(piece.getBox());
            });
        }

        std::optional<Box> branch(int32_t x, int32_t y, int32_t z, int32_t direction, int32_t depth) {
            const Box &room = mPieces.front().getBox();
            if (depth > 8 || std::abs(x - room.mMinX) > 80 || std::abs(z - room.mMinZ) > 80)
                return std::nullopt;

            const int32_t roll = draw(100);
            Box box;
            if (roll >= 80) {
                const bool tall = draw(4) == 0;
                box = Box::orient(x, y, z, -1, 0, 0, 5, tall ? 7 : 3, 5, direction);
                if (collides(box))
                    return std::nullopt;
                mPieces.emplace_back(Type::Crossing, box, direction, depth + 1);
                crossing(box, direction, depth + 1, tall);
            } else if (roll >= 70) {
                box = Box::orient(x, y, z, 0, -5, 0, 3, 8, 9, direction);
                if (collides(box))
                    return std::nullopt;
                mPieces.emplace_back(Type::Stairs, box, direction, depth + 1);
                forward(box, direction, box.mMinY, depth + 1);
            } else {
                int32_t sections = draw(3) + 2;
                do {
                    box = Box::orient(x, y, z, 0, 0, 0, 3, 3, sections * 5, direction);
                    if (!collides(box))
                        break;
                    --sections;
                } while (sections > 0);
                if (sections == 0)
                    return std::nullopt;
                Piece &piece = mPieces.emplace_back(Type::Corridor, box, direction, depth + 1);
                piece.mRails = draw(3) == 0;
                piece.mSpiders = !piece.mRails && draw(23) == 0;
                corridor(box, direction, depth + 1);
            }
            return box;
        }

        void forward(const Box &box, int32_t direction, int32_t y, int32_t depth) {
            switch (direction) {
                case SOUTH: branch(box.mMinX, y, box.mMaxZ + 1, SOUTH, depth); break;
                case WEST: branch(box.mMinX - 1, y, box.mMinZ, WEST, depth); break;
                case NORTH: branch(box.mMinX, y, box.mMinZ - 1, NORTH, depth); break;
                case EAST: branch(box.mMaxX + 1, y, box.mMinZ, EAST, depth); break;
            }
        }

        void corridor(const Box &box, int32_t direction, int32_t depth) {
            const int32_t turn = draw(4);
            const int32_t y = box.mMinY + draw(3) - 1;
            if (turn <= 1) {
                forward(box, direction, y, depth);
            } else if ((direction & 1) == 0) {
                const int32_t z = direction == SOUTH ? box.mMaxZ - 3 : box.mMinZ;
                if (turn == 2)
                    branch(box.mMinX - 1, y, z, WEST, depth);
                else
                    branch(box.mMaxX + 1, y, z, EAST, depth);
            } else {
                const int32_t x = direction == EAST ? box.mMaxX - 3 : box.mMinX;
                if (turn == 2)
                    branch(x, y, box.mMinZ - 1, NORTH, depth);
                else
                    branch(x, y, box.mMaxZ + 1, SOUTH, depth);
            }

            if (depth > 7)
                return;
            const bool alongZ = (direction & 1) == 0;
            const int32_t first = alongZ ? box.mMinZ : box.mMinX;
            const int32_t last = alongZ ? box.mMaxZ : box.mMaxX;
            for (int32_t offset = first + 6; offset <= last; offset += 5) {
                const int32_t side = draw(5);
                if (side > 1)
                    continue;
                if (alongZ) {
                    branch(side == 0 ? box.mMinX - 1 : box.mMaxX + 1, box.mMinY, offset - 3,
                           side == 0 ? WEST : EAST, depth + 1);
                } else {
                    branch(offset - 3, box.mMinY, side == 0 ? box.mMinZ - 1 : box.mMaxZ + 1,
                           side == 0 ? NORTH : SOUTH, depth + 1);
                }
            }
        }

        void crossing(const Box &box, int32_t direction, int32_t depth, bool tall) {
            const auto connect = [&](int32_t side, int32_t y) {
                switch (side) {
                    case NORTH: branch(box.mMinX + 1, y, box.mMinZ - 1, NORTH, depth); break;
                    case WEST: branch(box.mMinX - 1, y, box.mMinZ + 1, WEST, depth); break;
                    case EAST: branch(box.mMaxX + 1, y, box.mMinZ + 1, EAST, depth); break;
                    case SOUTH: branch(box.mMinX + 1, y, box.mMaxZ + 1, SOUTH, depth); break;
                }
            };
            constexpr std::array<std::array<int32_t, 3>, 4> exits{{
                    {SOUTH, WEST, EAST}, {NORTH, SOUTH, WEST}, {NORTH, WEST, EAST}, {NORTH, SOUTH, EAST}}};
            for (const int32_t side: exits[(size_t) direction])
                connect(side, box.mMinY);

            if (tall) {
                for (const int32_t side: {NORTH, WEST, EAST, SOUTH}) {
                    if ((GameRandom::nextUnsigned(mRandom) & 0x8000000U) != 0)
                        connect(side, box.mMinY + 4);
                }
            }
        }

        std::mt19937 &mRandom;
        std::vector<Piece> mPieces;
    };
}

std::optional<MineshaftStructure::Plan> MineshaftStructure::plan(const ChunkGenerator &generator, int32_t chunkX,
                                                               int32_t chunkZ) {
    std::mt19937 random;
    if (!MineshaftFeature::rollStart(generator.getSeed(), chunkX, chunkZ, random))
        return std::nullopt;

    const int32_t x = chunkX * 16;
    const int32_t z = chunkZ * 16;
    const int32_t biome = generator.pickBiome(x, 1, z);
    const bool mesa = biome == BiomeIds::MESA || biome == BiomeIds::MESA_BRYCE || biome == BiomeIds::MESA_PLATEAU_STONE;
    return Planner(random, mesa, x, z).finish(generator, mesa);
}
