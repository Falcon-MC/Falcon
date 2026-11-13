#include "World/Level/LevelGen/Structure/JigsawPlacer.h"

#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <algorithm>
#include <array>
#include <utility>

namespace {
    const std::array<std::array<int32_t, 3>, 6> FACING_STEPS = {
            {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
    const std::array<JigsawPlacer::Facing, 6> OPPOSITE = {1, 0, 3, 2, 5, 4};
    // the horizontal facings clockwise from north
    const std::array<JigsawPlacer::Facing, 4> CLOCKWISE = {2, 5, 3, 4};

    JigsawPlacer::Facing facingOf(const std::string &name) {
        static const char *NAMES[] = {"down", "up", "north", "south", "west", "east"};
        for (JigsawPlacer::Facing facing = 0; facing < 6; ++facing) {
            if (name == NAMES[facing])
                return facing;
        }
        return 0;
    }

    JigsawPlacer::Box boxAt(const JigsawPlacer::Template &source, int32_t quarterTurns, int32_t x, int32_t y,
                            int32_t z) {
        int32_t sizeX = source.mSizeX;
        int32_t sizeZ = source.mSizeZ;
        if ((quarterTurns & 1) != 0)
            std::swap(sizeX, sizeZ);

        const bool flippedX = quarterTurns == 1 || quarterTurns == 2;
        const bool flippedZ = quarterTurns == 2 || quarterTurns == 3;
        JigsawPlacer::Box box;
        box.mMinX = flippedX ? x + 1 - sizeX : x;
        box.mMaxX = flippedX ? x : x + sizeX - 1;
        box.mMinY = y;
        box.mMaxY = y + source.mSizeY - 1;
        box.mMinZ = flippedZ ? z + 1 - sizeZ : z;
        box.mMaxZ = flippedZ ? z : z + sizeZ - 1;
        return box;
    }

    bool contains(const JigsawPlacer::Box &box, int32_t x, int32_t y, int32_t z) {
        return x >= box.mMinX && x <= box.mMaxX && z >= box.mMinZ && z <= box.mMaxZ && y >= box.mMinY
               && y <= box.mMaxY;
    }

    // bounds and taken boxes are compared inclusively on both ends
    bool fits(const std::vector<JigsawPlacer::Box> &taken, const JigsawPlacer::Box &bounds,
              const JigsawPlacer::Box &box) {
        if (box.mMinX < bounds.mMinX || box.mMaxX > bounds.mMaxX || box.mMinY < bounds.mMinY
            || box.mMaxY > bounds.mMaxY || box.mMinZ < bounds.mMinZ || box.mMaxZ > bounds.mMaxZ)
            return false;

        for (const JigsawPlacer::Box &other: taken) {
            if (other.mMaxX >= box.mMinX && other.mMinX <= box.mMaxX && other.mMaxZ >= box.mMinZ
                && other.mMinZ <= box.mMaxZ && other.mMaxY >= box.mMinY && other.mMinY <= box.mMaxY)
                return false;
        }
        return true;
    }

    // a box one short of its corner on every axis is stored one block past it instead
    JigsawPlacer::Box takenOf(const JigsawPlacer::Box &box) {
        if (box.mMaxX + 1 != box.mMinX || box.mMaxY + 1 != box.mMinY || box.mMaxZ + 1 != box.mMinZ)
            return box;

        JigsawPlacer::Box grown = box;
        grown.mMaxX = box.mMinX + 1;
        grown.mMaxY = box.mMinY + 1;
        grown.mMaxZ = box.mMinZ + 1;
        return grown;
    }

    bool isEmptyPoolName(const std::string &name) {
        return name == "empty" || name == "minecraft:empty";
    }
}

JigsawPlacer::JigsawPlacer(NextInt nextInt, PoolLookup pools, TemplateLookup templates, Height height)
        : mNextInt(std::move(nextInt)), mPools(std::move(pools)), mTemplates(std::move(templates)),
          mHeight(std::move(height)) {}

JigsawPlacer::Facing JigsawPlacer::turn(Facing facing, int32_t quarterTurns) {
    if (facing < 2)
        return facing;

    const auto index = std::find(CLOCKWISE.begin(), CLOCKWISE.end(), facing) - CLOCKWISE.begin();
    return CLOCKWISE[(size_t) ((index + quarterTurns) & 3)];
}

bool JigsawPlacer::canAttach(const Jigsaw &parent, int32_t parentTurns, const Jigsaw &child, int32_t childTurns) {
    if (parent.mTarget != child.mName)
        return false;

    const Facing front = turn(parent.mFront, parentTurns);
    if (front != OPPOSITE[(size_t) turn(child.mFront, childTurns)])
        return false;

    // only an aligned joint facing up or down compares the tops; a rollable one turns freely
    if (front > 1 || !parent.mAligned)
        return true;

    return turn(parent.mTop, parentTurns) == turn(child.mTop, childTurns);
}

std::shared_ptr<const JigsawPlacer::Template> JigsawPlacer::templateOf(const StructureTemplate &source) {
    auto out = std::make_shared<Template>();
    out->mSizeX = source.getSizeX();
    out->mSizeY = source.getSizeY();
    out->mSizeZ = source.getSizeZ();
    for (const StructureTemplate::BlockEntry &block: source.getBlocks()) {
        const StructureTemplate::PaletteEntry &entry = source.getPalette()[(size_t) block.mPaletteIndex];
        if (entry.mName != "minecraft:jigsaw")
            continue;

        Jigsaw jigsaw;
        jigsaw.x = block.x;
        jigsaw.y = block.y;
        jigsaw.z = block.z;
        // orientation is the front, then the top, such as west_up
        const auto orientation = entry.mProperties.find("orientation");
        if (orientation != entry.mProperties.end()) {
            const size_t split = orientation->second.find('_');
            jigsaw.mFront = facingOf(orientation->second.substr(0, split));
            if (split != std::string::npos)
                jigsaw.mTop = facingOf(orientation->second.substr(split + 1));
        }

        const Tag &data = block.mData;
        jigsaw.mName = data.contains("name") ? data.getString("name") : data.getString("attachement_type");
        // the game targets the jigsaw's own name when the file names no target
        jigsaw.mTarget = data.contains("target") ? data.getString("target") : jigsaw.mName;
        jigsaw.mPool = data.contains("target_pool") ? data.getString("target_pool") : data.getString("pool");
        jigsaw.mFinalState = data.getString("final_state");
        jigsaw.mAligned = data.getString("joint") == "aligned";
        jigsaw.mPlacementPriority = data.getInt("placement_priority");
        jigsaw.mSelectionPriority = data.getInt("selection_priority");
        out->mJigsaws.push_back(std::move(jigsaw));
    }
    return out;
}

template <typename T>
void JigsawPlacer::_shuffle(std::vector<T> &values) {
    for (size_t size = values.size(); size > 1; --size) {
        const size_t other = (size_t) mNextInt((int32_t) size);
        std::swap(values[size - 1], values[other]);
    }
}

std::vector<size_t> JigsawPlacer::_shuffledJigsaws(const Template &source) {
    std::vector<size_t> order(source.mJigsaws.size());
    for (size_t index = 0; index < order.size(); ++index)
        order[index] = index;

    _shuffle(order);
    std::stable_sort(order.begin(), order.end(), [&](size_t left, size_t right) {
        return source.mJigsaws[left].mSelectionPriority > source.mJigsaws[right].mSelectionPriority;
    });
    return order;
}

// highest placement priority first, then the order the pieces were queued in
bool JigsawPlacer::_later(const Queued &left, const Queued &right) {
    return left.mPriority < right.mPriority || (left.mPriority == right.mPriority && left.mSequence < right.mSequence);
}

bool JigsawPlacer::_poolUsable(const std::string &name, const Pool *&pool) {
    pool = mPools(name);
    return pool != nullptr && (!pool->mSlots.empty() || isEmptyPoolName(name));
}

bool JigsawPlacer::place(const Settings &settings, Result &result) {
    mMaxDepth = settings.mMaxDepth;
    mPieces.clear();
    mShapes.clear();
    mQueue.clear();
    mSequence = 0;
    result = Result();

    const int32_t quarterTurns = mNextInt(4);
    const Pool *pool = mPools(settings.mStartPool);
    if (pool == nullptr || pool->mSlots.empty())
        return false;

    const Element *element = pool->mSlots[(size_t) mNextInt((int32_t) pool->mSlots.size())];
    if (element == nullptr)
        return false;

    const std::shared_ptr<const Template> source = mTemplates(*element);
    if (source == nullptr)
        return false;

    int32_t anchorX = settings.mStartX;
    int32_t anchorY = settings.mStartY;
    int32_t anchorZ = settings.mStartZ;
    if (!settings.mStartJigsaw.empty()) {
        bool found = false;
        for (const size_t index: _shuffledJigsaws(*source)) {
            const Jigsaw &jigsaw = source->mJigsaws[index];
            if (jigsaw.mName != settings.mStartJigsaw)
                continue;

            int32_t offsetX = jigsaw.x;
            int32_t offsetZ = jigsaw.z;
            StructureTemplate::rotate(quarterTurns, offsetX, offsetZ);
            anchorX += offsetX;
            anchorY += jigsaw.y;
            anchorZ += offsetZ;
            found = true;
            break;
        }
        if (!found)
            return false;
    }

    // the start piece is moved back by the anchor's offset, so the anchor jigsaw sits on the start position
    Piece start;
    start.mElement = element;
    start.x = settings.mStartX - (anchorX - settings.mStartX);
    start.y = settings.mStartY - (anchorY - settings.mStartY);
    start.z = settings.mStartZ - (anchorZ - settings.mStartZ);
    start.mQuarterTurns = quarterTurns;
    start.mBox = boxAt(*source, quarterTurns, start.x, start.y, start.z);

    const int32_t centreX = start.mBox.mMinX + (start.mBox.mMaxX - start.mBox.mMinX + 1) / 2;
    const int32_t centreZ = start.mBox.mMinZ + (start.mBox.mMaxZ - start.mBox.mMinZ + 1) / 2;
    const int32_t groundY = settings.mProjection == Projection::None
                                    ? start.y
                                    : settings.mStartY + mHeight(centreX, centreZ, settings.mProjection);
    const int32_t lift = groundY - (start.mBox.mMinY + start.mGroundLevelDelta);
    start.mBox.mMinY += lift;
    start.mBox.mMaxY += lift;
    start.y += lift;

    if ((settings.mPaddingTop | settings.mPaddingBottom) != 0
        && (settings.mDimensionMinY + settings.mPaddingBottom > start.mBox.mMinY
            || settings.mDimensionHeight - settings.mPaddingTop < start.mBox.mMaxY))
        return false;

    result.x = centreX;
    result.y = groundY + (anchorY - settings.mStartY);
    result.z = centreZ;
    if (settings.mMaxDepth <= 0)
        return true;

    mPieces.push_back(start);

    const int64_t horizontal = settings.mMaxDistanceHorizontal;
    const int64_t vertical = settings.mMaxDistanceVertical;
    Shape outer;
    outer.mBounds.mMinX = (int32_t) std::max<int64_t>((int64_t) result.x - horizontal, INT32_MIN);
    outer.mBounds.mMaxX = (int32_t) std::min<int64_t>((int64_t) result.x + horizontal, INT32_MAX - 1) + 1;
    outer.mBounds.mMinZ = (int32_t) std::max<int64_t>((int64_t) result.z - horizontal, INT32_MIN);
    outer.mBounds.mMaxZ = (int32_t) std::min<int64_t>((int64_t) result.z + horizontal, INT32_MAX - 1) + 1;
    outer.mBounds.mMinY = (int32_t) std::max<int64_t>((int64_t) result.y - vertical,
                                                      (int64_t) settings.mPaddingBottom + settings.mDimensionMinY);
    outer.mBounds.mMaxY = (int32_t) std::min<int64_t>((int64_t) result.y + vertical + 1,
                                                      (int64_t) settings.mDimensionHeight - settings.mPaddingTop);
    outer.mTaken.push_back(takenOf(start.mBox));
    mShapes.push_back(std::move(outer));

    Queued first;
    first.mSequence = --mSequence;
    mQueue.push_back(first);
    while (!mQueue.empty()) {
        std::pop_heap(mQueue.begin(), mQueue.end(), &JigsawPlacer::_later);
        const Queued entry = mQueue.back();
        mQueue.pop_back();
        _expand(entry);
    }

    result.mPieces = std::move(mPieces);
    return true;
}

void JigsawPlacer::_expand(const Queued &entry) {
    const Piece parent = mPieces[entry.mPiece];
    const Element &element = *parent.mElement;
    const std::shared_ptr<const Template> source = mTemplates(element);
    if (source == nullptr)
        return;

    size_t inner = SIZE_MAX;
    for (const size_t index: _shuffledJigsaws(*source)) {
        const Jigsaw &jigsaw = source->mJigsaws[index];
        const Facing facing = turn(jigsaw.mFront, parent.mQuarterTurns);
        int32_t offsetX = jigsaw.x;
        int32_t offsetZ = jigsaw.z;
        StructureTemplate::rotate(parent.mQuarterTurns, offsetX, offsetZ);
        const int32_t jigsawX = parent.x + offsetX;
        const int32_t jigsawY = parent.y + jigsaw.y;
        const int32_t jigsawZ = parent.z + offsetZ;
        const std::array<int32_t, 3> &step = FACING_STEPS[(size_t) facing];
        const int32_t targetX = jigsawX + step[0];
        const int32_t targetY = jigsawY + step[1];
        const int32_t targetZ = jigsawZ + step[2];

        // a missing pool, or a pool without elements unless it is the empty one, leaves the jigsaw alone
        const Pool *pool = nullptr;
        const Pool *fallback = nullptr;
        if (!_poolUsable(jigsaw.mPool, pool) || !_poolUsable(pool->mFallback, fallback))
            continue;

        size_t shape = entry.mShape;
        if (contains(parent.mBox, targetX, targetY, targetZ)) {
            if (inner == SIZE_MAX) {
                Shape inside;
                inside.mBounds = parent.mBox;
                mShapes.push_back(std::move(inside));
                inner = mShapes.size() - 1;
            }
            shape = inner;
        }

        std::vector<const Element *> candidates;
        if (entry.mDepth != mMaxDepth) {
            std::vector<const Element *> drawn = pool->mSlots;
            _shuffle(drawn);
            candidates = std::move(drawn);
        }
        std::vector<const Element *> fallbacks = fallback->mSlots;
        _shuffle(fallbacks);
        candidates.insert(candidates.end(), fallbacks.begin(), fallbacks.end());

        const int32_t stepY = step[1];
        const int32_t relativeY = jigsawY - parent.mBox.mMinY;
        int32_t surface = INT32_MAX;
        for (const Element *candidate: candidates) {
            if (candidate == nullptr || candidate->mLocation.empty())
                break;

            const std::shared_ptr<const Template> other = mTemplates(*candidate);
            if (other == nullptr)
                break;

            std::array<int32_t, 4> turns = {0, 1, 2, 3};
            for (int32_t size = 4; size > 1; --size)
                std::swap(turns[(size_t) (size - 1)], turns[(size_t) mNextInt(size)]);

            bool placed = false;
            for (const int32_t childTurns: turns) {
                for (const size_t childIndex: _shuffledJigsaws(*other)) {
                    const Jigsaw &match = other->mJigsaws[childIndex];
                    if (!canAttach(jigsaw, parent.mQuarterTurns, match, childTurns))
                        continue;

                    int32_t matchX = match.x;
                    int32_t matchZ = match.z;
                    StructureTemplate::rotate(childTurns, matchX, matchZ);
                    const int32_t deltaY = relativeY - match.y + stepY;
                    int32_t minY;
                    if (!element.mTerrainMatching && !candidate->mTerrainMatching) {
                        minY = parent.mBox.mMinY + deltaY;
                    } else {
                        if (surface == INT32_MAX)
                            surface = mHeight(jigsawX, jigsawZ, Projection::WorldSurface);
                        minY = surface - match.y;
                    }

                    const int32_t pieceX = targetX - matchX;
                    const int32_t pieceZ = targetZ - matchZ;
                    const Box box = boxAt(*other, childTurns, pieceX, minY, pieceZ);
                    Shape &free = mShapes[shape];
                    if (!fits(free.mTaken, free.mBounds, box))
                        continue;

                    free.mTaken.push_back(takenOf(box));

                    Piece child;
                    child.mElement = candidate;
                    child.x = pieceX;
                    child.y = minY;
                    child.z = pieceZ;
                    child.mQuarterTurns = childTurns;
                    child.mDepth = entry.mDepth + 1;
                    child.mBox = box;
                    child.mGroundLevelDelta = candidate->mTerrainMatching ? 1 : parent.mGroundLevelDelta - deltaY;

                    int32_t junctionY;
                    if (!element.mTerrainMatching) {
                        junctionY = jigsawY;
                    } else if (!candidate->mTerrainMatching) {
                        junctionY = minY + match.y;
                    } else {
                        if (surface == INT32_MAX)
                            surface = mHeight(jigsawX, jigsawZ, Projection::WorldSurface);
                        junctionY = surface + deltaY / 2;
                    }

                    mPieces[entry.mPiece].mJunctions.push_back(
                            Junction{targetX, junctionY - relativeY + parent.mGroundLevelDelta, targetZ, deltaY,
                                     candidate->mTerrainMatching});
                    child.mJunctions.push_back(Junction{jigsawX, junctionY - match.y + child.mGroundLevelDelta,
                                                        jigsawZ, -deltaY, element.mTerrainMatching});
                    mPieces.push_back(std::move(child));

                    if (entry.mDepth < mMaxDepth) {
                        Queued next;
                        next.mPriority = jigsaw.mPlacementPriority;
                        next.mSequence = --mSequence;
                        next.mPiece = mPieces.size() - 1;
                        next.mShape = shape;
                        next.mDepth = entry.mDepth + 1;
                        mQueue.push_back(next);
                        std::push_heap(mQueue.begin(), mQueue.end(), &JigsawPlacer::_later);
                    }
                    placed = true;
                    break;
                }
                if (placed)
                    break;
            }
            if (placed)
                break;
        }
    }
}
