#include "World/Level/LevelGen/Structure/JigsawAssembler.h"

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <utility>

namespace {
    const std::array<std::array<int32_t, 3>, 6> FACING_STEPS = {
            {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
    const std::array<int32_t, 6> OPPOSITE = {1, 0, 3, 2, 5, 4};
    // the horizontal facings clockwise from north
    const std::array<int32_t, 4> CLOCKWISE = {2, 5, 3, 4};

    int32_t facingOf(const std::string &name) {
        if (name == "down")
            return 0;
        if (name == "up")
            return 1;
        if (name == "north")
            return 2;
        if (name == "south")
            return 3;
        if (name == "west")
            return 4;
        return 5;
    }

    int32_t rotateFacing(int32_t facing, int32_t quarterTurns) {
        if (facing < 2)
            return facing;

        const auto index = std::find(CLOCKWISE.begin(), CLOCKWISE.end(), facing) - CLOCKWISE.begin();
        return CLOCKWISE[(size_t) ((index + quarterTurns) & 3)];
    }

    void rotate(int32_t quarterTurns, int32_t &x, int32_t &z) {
        for (int32_t i = 0; i < quarterTurns; ++i) {
            const int32_t oldX = x;
            x = -z;
            z = oldX;
        }
    }

    bool overlapsFlat(const JigsawBox &a, const JigsawBox &b) {
        return a.mMaxX >= b.mMinX && a.mMinX <= b.mMaxX && a.mMaxZ >= b.mMinZ && a.mMinZ <= b.mMaxZ;
    }

    std::shared_ptr<const StructureTemplate> templateOf(const JigsawElementData &element) {
        return element.mTemplate == nullptr ? nullptr : StructureTemplate::load(element.mTemplate);
    }
}

JigsawAssembler::JigsawAssembler(const ChunkGenerator &generator, std::mt19937 &random, int32_t maxDepth, int32_t seaLevel)
    : mGenerator(generator), mRandom(random), mMaxDepth(maxDepth), mSeaLevel(seaLevel) {}

template <typename T>
void JigsawAssembler::_shuffle(std::vector<T> &values) {
    for (size_t i = 1; i < values.size(); ++i) {
        const size_t other = mRandom() % (uint32_t) (i + 1);
        if (other != i)
            std::swap(values[i], values[other]);
    }
}

int32_t JigsawAssembler::_groundHeight(int32_t x, int32_t z) {
    return std::max(mGenerator.getPreliminarySurfaceLevel(x >> 2, z >> 2), mSeaLevel);
}

std::vector<JigsawAssembler::Marker> JigsawAssembler::_markers(const JigsawElementData &element, int32_t x, int32_t y,
                                                               int32_t z, int32_t quarterTurns) const {
    std::vector<Marker> markers;
    if (element.mFeature != nullptr) {
        Marker marker;
        marker.x = x;
        marker.y = y;
        marker.z = z;
        marker.mName = "minecraft:bottom";
        marker.mPool = "minecraft:empty";
        markers.push_back(marker);
        return markers;
    }

    const std::shared_ptr<const StructureTemplate> file = templateOf(element);
    if (file == nullptr)
        return markers;

    for (const StructureTemplate::BlockEntry &block : file->getBlocks()) {
        const StructureTemplate::PaletteEntry &entry = file->getPalette()[(size_t) block.mPaletteIndex];
        if (entry.mName != "minecraft:jigsaw")
            continue;

        int32_t offsetX = block.x;
        int32_t offsetZ = block.z;
        rotate(quarterTurns, offsetX, offsetZ);

        Marker marker;
        marker.x = x + offsetX;
        marker.y = y + block.y;
        marker.z = z + offsetZ;
        const auto facing = entry.mProperties.find("facing");
        marker.mFacing = rotateFacing(facing == entry.mProperties.end() ? 0 : facingOf(facing->second), quarterTurns);
        // the shipped files spell it this way and use it as both the name and the target of the jigsaw
        marker.mName = block.mData.getString("attachement_type", "");
        marker.mPool = block.mData.getString("target_pool", "");
        markers.push_back(std::move(marker));
    }

    return markers;
}

JigsawBox JigsawAssembler::_box(const JigsawElementData &element, int32_t x, int32_t y, int32_t z,
                                int32_t quarterTurns) const {
    const std::shared_ptr<const StructureTemplate> file = templateOf(element);
    int32_t sizeX = file == nullptr ? 0 : file->getSizeX();
    const int32_t sizeY = file == nullptr ? 0 : file->getSizeY();
    int32_t sizeZ = file == nullptr ? 0 : file->getSizeZ();
    if ((quarterTurns & 1) != 0)
        std::swap(sizeX, sizeZ);

    const bool flippedX = quarterTurns == 1 || quarterTurns == 2;
    const bool flippedZ = quarterTurns == 2 || quarterTurns == 3;
    JigsawBox box;
    box.mMinX = flippedX ? x + 1 - sizeX : x;
    box.mMaxX = flippedX ? x : x + sizeX - 1;
    box.mMinY = y;
    box.mMaxY = y + sizeY - 1;
    box.mMinZ = flippedZ ? z + 1 - sizeZ : z;
    box.mMaxZ = flippedZ ? z : z + sizeZ - 1;
    return box;
}

std::vector<JigsawPiece> JigsawAssembler::assemble(const JigsawElementData &start, int32_t x, int32_t y, int32_t z,
                                                   int32_t quarterTurns) {
    mPieces.clear();

    JigsawPiece piece;
    piece.mElement = &start;
    piece.x = x;
    piece.z = z;
    piece.mQuarterTurns = quarterTurns;
    piece.mBox = _box(start, x, y, z, quarterTurns);

    const int32_t middleX = piece.mBox.mMinX + (piece.mBox.mMaxX - piece.mBox.mMinX + 1) / 2;
    const int32_t middleZ = piece.mBox.mMinZ + (piece.mBox.mMaxZ - piece.mBox.mMinZ + 1) / 2;
    const int32_t lift = _groundHeight(middleX, middleZ) - 1 - piece.mBox.mMinY;
    piece.y = y + lift;
    piece.mBox.mMinY += lift;
    piece.mBox.mMaxY += lift;

    mPieces.push_back(piece);
    _expand(0);
    return std::move(mPieces);
}

void JigsawAssembler::_expand(size_t pieceIndex) {
    const JigsawPiece piece = mPieces[pieceIndex];
    if (piece.mDepth > mMaxDepth)
        return;

    std::vector<Marker> markers = _markers(*piece.mElement, piece.x, piece.y, piece.z, piece.mQuarterTurns);
    _shuffle(markers);

    std::vector<JigsawBox> inside;
    for (const Marker &marker : markers) {
        const JigsawPools::Pool *pool = JigsawPools::get(marker.mPool);
        if (pool == nullptr || pool->mSlots.empty())
            continue;

        const JigsawPools::Pool *fallback = JigsawPools::get(pool->mFallback);
        const std::array<int32_t, 3> &step = FACING_STEPS[(size_t) marker.mFacing];
        const int32_t target[3] = {marker.x + step[0], marker.y + step[1], marker.z + step[2]};

        if (piece.mDepth != mMaxDepth && _tryPool(pieceIndex, marker, *pool, target, inside))
            continue;

        if (fallback != nullptr && !fallback->mSlots.empty())
            _tryPool(pieceIndex, marker, *fallback, target, inside);
    }
}

bool JigsawAssembler::_tryPool(size_t pieceIndex, const Marker &marker, const JigsawPools::Pool &pool,
                               const int32_t target[3], std::vector<JigsawBox> &inside) {
    const JigsawPiece parent = mPieces[pieceIndex];
    const JigsawBox &parentBox = parent.mBox;
    const bool targetInside = target[0] >= parentBox.mMinX && target[0] <= parentBox.mMaxX
                              && target[2] >= parentBox.mMinZ && target[2] <= parentBox.mMaxZ
                              && target[1] >= parentBox.mMinY && target[1] <= parentBox.mMaxY;
    const int32_t groundBelow = _groundHeight(marker.x, marker.z) - 1;

    std::vector<size_t> slots(pool.mSlots.size());
    std::iota(slots.begin(), slots.end(), (size_t) 0);
    _shuffle(slots);

    for (const size_t slot : slots) {
        const JigsawElementData *element = pool.mSlots[slot];
        // the empty element is a valid end of the branch
        if (element == nullptr)
            return true;

        std::array<int32_t, 4> turns = {1, 2, 3, 0};
        if ((mRandom() & 1) == 0)
            std::swap(turns[0], turns[1]);
        const uint32_t third = mRandom() % 3;
        if (third != 2)
            std::swap(turns[2], turns[third]);
        const uint32_t fourth = mRandom() & 3;
        if (fourth != 3)
            std::swap(turns[3], turns[fourth]);

        for (const int32_t quarterTurns : turns) {
            std::vector<Marker> candidates = _markers(*element, 0, 0, 0, quarterTurns);
            _shuffle(candidates);

            for (const Marker &candidate : candidates) {
                if (candidate.mName != marker.mName || marker.mFacing != OPPOSITE[(size_t) candidate.mFacing])
                    continue;

                const int32_t placeX = target[0] - candidate.x;
                const int32_t placeZ = target[2] - candidate.z;

                const int32_t step = marker.mFacing == 1 ? 1 : marker.mFacing == 0 ? -1 : 0;
                const bool rigid = !parent.mElement->mTerrainMatching && !element->mTerrainMatching;
                const int32_t base = rigid ? parentBox.mMinY : groundBelow;
                const int32_t minY = step + base + marker.y - (candidate.y + parentBox.mMinY);

                JigsawBox box = _box(*element, placeX, minY, placeZ, quarterTurns);
                bool free = true;
                if (targetInside) {
                    free = box.mMinX >= parentBox.mMinX && box.mMaxX <= parentBox.mMaxX
                           && box.mMinZ >= parentBox.mMinZ && box.mMaxZ <= parentBox.mMaxZ;
                    for (const JigsawBox &other : inside)
                        free = free && !overlapsFlat(box, other);
                } else {
                    for (const JigsawPiece &other : mPieces)
                        free = free && !overlapsFlat(box, other.mBox);
                }

                if (!free)
                    continue;

                if (targetInside)
                    inside.push_back(box);

                JigsawPiece piece;
                piece.mElement = element;
                piece.x = placeX;
                piece.y = minY;
                piece.z = placeZ;
                piece.mQuarterTurns = quarterTurns;
                piece.mDepth = parent.mDepth + 1;
                piece.mBox = box;
                mPieces.push_back(piece);
                _expand(mPieces.size() - 1);
                return true;
            }
        }
    }

    return false;
}
