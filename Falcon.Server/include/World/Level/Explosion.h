#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class BlockState;
class Explosion;
class ItemActor;
class Level;
class ServerActor;
class ServerNetworkHandler;

// the blocks around the explosions of a tick, read once and kept while no chunk under them changes: an air
// cell answers without a level lookup
class BlastField {
public:
    // reads the blocks again unless the field at hand still holds everything this explosion reaches
    // upcoming: where the next explosions of the tick are, a wide read is worth it when many are close
    // targets: whether an entity in reach will ask how exposed it is; without one the rays alone are served,
    // from a smaller read
    // needLow and needHigh: the blocks this explosion will ask about, the high corner included;
    // sums: whether the targets are many enough for the table behind isBoxClear to pay for itself
    void prepare(Level &level, const Vector3f &source, double size, const Vector3i &needLow,
                 const Vector3i &needHigh, bool sums, const Vector3f *upcoming, size_t upcomingCount);

    // whether what was read still stands and holds that box of blocks, corners included
    bool holds(Level &level, const Vector3i &low, const Vector3i &high) const {
        return _stillHolds(level, low, high);
    }

    // that box lies within the blocks read, whatever happened to the level since
    bool contains(const Vector3i &low, const Vector3i &high) const {
        return mRead && mCovers && low.x >= mOriginX && low.y >= mOriginY && low.z >= mOriginZ
               && high.x < mOriginX + mDimX && high.y < mOriginY + mDimY && high.z < mOriginZ + mDimZ;
    }

    // the rays of one explosion begin: no block is marked yet
    void beginRays() {
        ++mRayStamp;
        if (mRayStamps.size() < mStates.size())
            mRayStamps.assign(mStates.size(), 0);
    }

    // true the first time a ray of the explosion at hand reaches that block
    bool markReached(int32_t index) {
        if (mRayStamps[(size_t) index] == mRayStamp)
            return false;
        mRayStamps[(size_t) index] = mRayStamp;
        return true;
    }

    // one read for a whole tick of explosions that break blocks: no shape kept, the sums made at once. It is
    // then kept true by patch() as blocks change, and read by several threads, each around its own explosion
    void readForMany(Level &level, const Vector3i &low, const Vector3i &high);

    // the block at that place changed: read again. True when a block now stands where the sums saw none,
    // which makes them wrong until resum()
    bool patch(Level &level, int32_t x, int32_t y, int32_t z);

    void resum();

    // the box holding every block within that many blocks of a center, as findBlocksInReach; false when the
    // field does not hold the whole reach
    bool scanBlocks(const Vector3i &center, int32_t reach, Vector3i &low, Vector3i &high, bool &any) const;

    // true the first time that block is reached under that stamp; stamps are handed out by nextRayStamp()
    bool markReachedWith(int32_t index, uint32_t stamp) {
        if (mRayStamps[(size_t) index] == stamp)
            return false;
        mRayStamps[(size_t) index] = stamp;
        return true;
    }

    static int32_t rayReach(double size);

    static int32_t targetReach(double size);

    // the box holding every block in reach of the rays, high corner included; false when there is none: no
    // ray meets a block. Without a read covering the reach the box is the whole reach
    bool findBlocksInReach(Vector3i &low, Vector3i &high) const;

    // -1 outside the field; the caller falls back to the level
    int32_t indexOf(int32_t x, int32_t y, int32_t z) const {
        const uint32_t localX = (uint32_t) (x - mOriginX);
        const uint32_t localY = (uint32_t) (y - mOriginY);
        const uint32_t localZ = (uint32_t) (z - mOriginZ);
        if (!mCovers || localX >= (uint32_t) mDimX || localY >= (uint32_t) mDimY || localZ >= (uint32_t) mDimZ)
            return -1;
        return (int32_t) ((localX * (uint32_t) mDimZ + localZ) * (uint32_t) mDimY + localY);
    }

    // null for air, an unloaded chunk or a height outside the world
    const BlockState *stateAt(int32_t index) const {
        return mStates[(size_t) index];
    }

    // false for a block nothing collides with: water, grass
    bool isBlocking(int32_t index) const {
        return mBlocking[(size_t) index] != 0;
    }

    // a whole cube holds any point of its cell: no shape to ask
    bool isFullCube(int32_t index) const {
        return mBlocking[(size_t) index] == FULL_CUBE;
    }

    // no blocking block between the two corners, both inside the field
    bool isBoxClear(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ) const {
        const int32_t lowX = minX - mOriginX;
        const int32_t lowY = minY - mOriginY;
        const int32_t lowZ = minZ - mOriginZ;
        const int32_t highX = maxX - mOriginX + 1;
        const int32_t highY = maxY - mOriginY + 1;
        const int32_t highZ = maxZ - mOriginZ + 1;
        if (!mCovers || !mSummed || lowX < 0 || lowY < 0 || lowZ < 0 || highX > mDimX || highY > mDimY
            || highZ > mDimZ)
            return false;
        return _count(mBlockingPrefix, lowX, lowY, lowZ, highX, highY, highZ) == 0;
    }

    bool isPositionInside(int32_t index, int32_t x, int32_t y, int32_t z, float pointX, float pointY, float pointZ);

    // read from a helper thread: a packed chunk is left alone, and the read reported as failed
    void setHelperOwned(bool helperOwned) {
        mHelperOwned = helperOwned;
    }

    bool hasFailed() const {
        return mFailed;
    }

    // the ray sampled as the level walk samples it: 1 stopped by a whole cube, 0 free, -1 when a sample
    // needs a shape or leaves the field
    int walk(double srcX, double srcY, double srcZ, double dstX, double dstY, double dstZ, double stepSize) const;

    // -1 for a block no explosion breaks, else the stronger of the two layers; read with the blocks, so
    // that the rays of one explosion can be cast from several threads
    float resistanceAt(int32_t index) const {
        return mResistances[(size_t) index];
    }

private:
    static const uint8_t PARTIAL = 1;
    static const uint8_t FULL_CUBE = 2;
    static const uint8_t OUTSIDE = 3;

    struct ChunkMark {
        int32_t mX;
        int32_t mZ;
        const void *mChunk;
        uint32_t mRevision;
    };

    size_t _prefix(int32_t x, int32_t y, int32_t z) const {
        return (size_t) ((x * (mDimZ + 1) + z) * (mDimY + 1) + y);
    }

    // blocks counted between local corners, the high one excluded
    int32_t _count(const std::vector<int32_t> &prefix, int32_t lowX, int32_t lowY, int32_t lowZ, int32_t highX,
                   int32_t highY, int32_t highZ) const {
        if (prefix.empty())
            return 0;
        return prefix[_prefix(highX, highY, highZ)] - prefix[_prefix(lowX, highY, highZ)]
               - prefix[_prefix(highX, highY, lowZ)] - prefix[_prefix(highX, lowY, highZ)]
               + prefix[_prefix(lowX, highY, lowZ)] + prefix[_prefix(lowX, lowY, highZ)]
               + prefix[_prefix(highX, lowY, lowZ)] - prefix[_prefix(lowX, lowY, lowZ)];
    }

    bool _stillHolds(Level &level, const Vector3i &low, const Vector3i &high) const;

    // low and high: block corners of the read, the high one included
    void _read(Level &level, const Vector3i &low, const Vector3i &high);

    void _sum(const std::vector<uint8_t> &cells, std::vector<int32_t> &prefix) const;

    void _noteColumn(int32_t x, int32_t z, int32_t low, int32_t high);

    bool mRead = false;
    bool mCovers = false;
    bool mHelperOwned = false;
    bool mFailed = false;
    bool mKeepShapes = true;
    // the sums behind isBoxClear, made only for an explosion with targets
    bool mSummed = false;
    bool mAnyBlocking = false;
    // whether the read at hand is a wide one, how many explosions it served, how many wide ones were lost
    bool mWide = false;
    size_t mWideServed = 0;
    int mWideWasted = 0;
    // the box of every block read, in world coordinates
    bool mAnyBlock = false;
    Vector3i mBlocksLow;
    Vector3i mBlocksHigh;
    const Level *mLevel = nullptr;
    int32_t mOriginX = 0;
    int32_t mOriginY = 0;
    int32_t mOriginZ = 0;
    int32_t mDimX = 0;
    int32_t mDimY = 0;
    int32_t mDimZ = 0;
    // local corners of what the explosion at hand reaches, the high one excluded
    int32_t mReachMinX = 0;
    int32_t mReachMinY = 0;
    int32_t mReachMinZ = 0;
    int32_t mReachMaxX = 0;
    int32_t mReachMaxY = 0;
    int32_t mReachMaxZ = 0;
    uint32_t mStamp = 0;
    uint32_t mRayStamp = 0;
    std::vector<uint32_t> mRayStamps;
    std::vector<ChunkMark> mChunks;
    std::vector<const BlockState *> mStates;
    std::vector<uint8_t> mBlocking;
    std::unordered_map<const BlockState *, uint8_t> mBlockingStates;
    std::unordered_map<const BlockState *, float> mResistStates;
    std::vector<int32_t> mBlockingPrefix;
    std::vector<uint32_t> mShapeStamps;
    std::vector<AxisAlignedBB> mShapes;
    std::vector<uint32_t> mResistanceStamps;
    std::vector<float> mResistances;
};

// a block a ray reached with force left, by its place in the blocks read (-1 outside them); the same block
// comes up from many rays
struct BlastReach {
    int32_t mIndex;
    Vector3i mBlock;
};

// the actors and items around the explosions of one tick, bucketed once: nothing moves between them
class ExplosionBatch {
public:
    struct Target {
        ServerActor *mActor;
        Vector3f mPosition;
        float mWidth;
        float mHeight;
        bool mInvulnerable;
        // its place in the lists that run alongside the buckets
        uint32_t mIndex;
    };

    // an explosion that leaves every block in place: all there is to it is who it pushes and hurts
    struct QuietBlast {
        Vector3f mSource;
        double mSize;
        const ServerActor *mSourceActor;
    };

    // a run of targets of one bucket, small enough to be one share of work
    struct Span {
        const Target *mFirst;
        size_t mCount;
    };

    // a target the explosion at hand has to settle on the main thread: it takes damage, or a block on the
    // way has a shape to ask
    struct Hit {
        const Target *mTarget;
        double mDistance;
        float mDensity;
    };

    explicit ExplosionBatch(ServerNetworkHandler &owner) : mOwner(owner) {}

    ~ExplosionBatch();

    // a tick of many explosions keeps the helper threads awake from the first to the last
    void setCrowded(bool crowded);

    bool isCrowded() const {
        return mCrowded;
    }

    // the owner lists the actors joining meanwhile: they are added to the buckets, not the buckets made again
    void setFollowingJoins(bool following) {
        mFollowingJoins = following;
    }

    std::vector<Hit> &getHits() {
        return mHits;
    }

    // the blocks each share of the rays reached, kept between explosions
    std::vector<std::vector<BlastReach>> &getRaySlices() {
        return mRaySlices;
    }

    // the pushes of explosions that break nothing, worked out on every core at once: they read the same
    // blocks and each moves actors nobody else is asked about. What a push cannot settle alone (a target
    // that takes damage, a shaped block in the way) is kept per explosion for the caller
    void settleQuiet(Level &level, const std::vector<QuietBlast> &blasts);

    // explosions that break blocks, several worked out at a time when they stand too far apart to see one
    // another, while the calling thread ends them one by one. Each still sees what the ones before it,
    // within its reach, broke
    void runApart(Level &level, const std::vector<Explosion *> &explosions);

    // the blocks read for the explosions worked out apart, while they are kept true
    const BlastField *getApartField() const {
        return mApartLive && !mApartStale ? &mApartField : nullptr;
    }

    // false for an explosion settleQuiet left whole to the caller
    bool isSettled(size_t blast) const {
        return blast < mQuietSettled.size() && mQuietSettled[blast] != 0;
    }

    std::vector<Hit> &getQuietHits(size_t blast) {
        return mQuietHits[blast];
    }

    // the buckets in reach, cut in shares; returns how many targets they hold
    size_t collectSpans(Level &level, const Vector3f &center, float reach, std::vector<Span> &spans);

    std::vector<Span> &getSpans() {
        return mSpans;
    }


    BlastField &getField() {
        return mField;
    }

    // the centers of the explosions still to come this tick, the one at hand first
    void setUpcoming(const Vector3f *centers, size_t count) {
        mUpcoming = centers;
        mUpcomingCount = count;
    }

    const Vector3f *getUpcoming() const {
        return mUpcoming;
    }

    size_t getUpcomingCount() const {
        return mUpcomingCount;
    }

    template <typename Visitor>
    void forEachItem(Level &level, const Vector3f &center, float reach, Visitor &&visit) {
        _syncItems(level);
        _forEachCell(mItems, center, reach, visit);
    }

private:
    static const int32_t CELL_SHIFT = 2;
    static const size_t SPAN = 8;

    static int64_t _cell(int32_t cellX, int32_t cellZ) {
        return (int64_t) (((uint64_t) (uint32_t) cellX << 32) | (uint64_t) (uint32_t) cellZ);
    }

    static int64_t _cellOf(const Vector3f &position);

    template <typename Cells, typename Visitor>
    static void _forEachCell(Cells &cells, const Vector3f &center, float reach, Visitor &&visit) {
        const int32_t minX = (int32_t) std::floor(center.x - reach) >> CELL_SHIFT;
        const int32_t maxX = (int32_t) std::floor(center.x + reach) >> CELL_SHIFT;
        const int32_t minZ = (int32_t) std::floor(center.z - reach) >> CELL_SHIFT;
        const int32_t maxZ = (int32_t) std::floor(center.z + reach) >> CELL_SHIFT;
        for (int32_t cellX = minX; cellX <= maxX; ++cellX) {
            for (int32_t cellZ = minZ; cellZ <= maxZ; ++cellZ) {
                const auto found = cells.find(_cell(cellX, cellZ));
                if (found == cells.end())
                    continue;
                // by index: a visitor may spawn, never remove
                for (size_t index = 0; index < found->second.size(); ++index)
                    visit(found->second[index]);
            }
        }
    }

    void _syncTargets(Level &level);

    void _addTarget(ServerActor &actor);

    void _applyChanges(Level &level);

    void _syncItems(Level &level);

    ServerNetworkHandler &mOwner;
    BlastField mField;
    bool mCrowded = false;
    bool mFollowingJoins = false;
    size_t mJoinsRead = 0;
    // actors that came and left between two looks at the changes
    std::vector<ServerActor *> mSkippedLeavers;
    const Vector3f *mUpcoming = nullptr;
    size_t mUpcomingCount = 0;
    std::vector<Hit> mHits;
    BlastField mApartField;
    bool mApartLive = false;
    bool mApartStale = false;
    std::vector<std::vector<BlastReach>> mRaySlices;
    std::vector<uint8_t> mQuietSettled;
    std::vector<std::vector<Hit>> mQuietHits;
    uint32_t mTargetCount = 0;
    std::vector<Span> mSpans;
    const Level *mTargetLevel = nullptr;
    uint64_t mTargetSerial = 0;
    std::unordered_map<int64_t, std::vector<Target>> mTargets;
    const Level *mItemLevel = nullptr;
    size_t mItemsIndexed = 0;
    std::unordered_map<int64_t, std::vector<ItemActor *>> mItems;
};

// explodeA: ray-cast the blocks, plugins may cancel or resize; explodeB: destruction, damage, effects
class Explosion {
public:
    Explosion(ServerNetworkHandler &owner, Level &level, const Vector3f &center, double size,
              const ServerActor *sourceActor, bool fromTnt);

    void setFireChance(double fireChance) {
        mFireChance = fireChance;
    }

    void setIncendiary(bool incendiary);

    // replaces the in-water check at the center
    void setInWater(bool inWater) {
        mInWater = inWater;
    }

    // shares the actor buckets and the scratch field with the other explosions of the tick
    void setBatch(ExplosionBatch *batch) {
        mBatch = batch;
    }

    // its pushes were worked out by the batch, as the blast of that rank
    void setSettledAs(size_t blast) {
        mQuietBlast = blast;
    }

    double getSize() const {
        return mSize;
    }

    bool explode();

    bool explodeWithoutBlocks();

    bool explodeA();

    bool explodeB();

    // an explosion centered in water or in a chunk not loaded leaves every block in place
    static bool breaksNothingAt(Level &level, const Vector3f &center);

    // no block at all where the rays of an explosion of that size can reach: nothing but air around
    static bool isReachEmpty(Level &level, const Vector3f &center, double size);

    static float getBlockDensity(Level &level, const Vector3f &source, const AxisAlignedBB &boundingBox);

    friend class ExplosionBatch;

    static bool isRayCollidingWithBlocks(Level &level, double srcX, double srcY, double srcZ, double dstX, double dstY,
                                         double dstZ, double stepSize);

private:
    static int64_t _key(const Vector3i &position);

    bool _findAffectedBlocks();

    using Reach = BlastReach;

    // the rays whose first cube coordinate is in [from, to); false when one left the blocks read and has to
    // be cast again from the tick thread
    bool _castRays(const BlastField &field, int from, int to, const Vector3i &blocksLow, const Vector3i &blocksHigh,
                   bool helper, std::vector<Reach> &reached) const;

    bool _allowByPlugins();

    // the explosion before event: scripts may cancel or narrow the blocks
    bool _allowByScripts();

    static float _calculateEntityDamage(double doubleRadius, double impact);

    float _scaleDamageForDifficulty(float damage) const;

    void _damageEntities();

    ExplosionBatch &_batch();

    // what the blocks are read for: the rays, the actors counted in reach, or any box around the source
    enum class FieldUse { Rays, Targets, Anything };

    BlastField &_field(FieldUse use, bool sums = true);

    // actors in reach, counted once
    size_t _near();

    float _density(const AxisAlignedBB &boundingBox);

    // 1 when nothing can hide the target, -1 when the rays have to be walked
    float _quickDensity(const AxisAlignedBB &boundingBox) const;

    // from any thread: reads the field alone, -1 when it meets a block whose shape has to be asked
    float _walkedDensity(const AxisAlignedBB &boundingBox) const;

    bool _isInsideWater(const Vector3f &position) const;

    void _destroyBlocks();

    void _ignite();

    void _playEffects();

    ServerNetworkHandler &mOwner;
    Level &mLevel;
    Vector3f mSource;
    double mSize;
    double mFireChance = 0.0;
    const ServerActor *mSourceActor;
    bool mFromTnt;
    std::optional<bool> mInWater;
    std::vector<Vector3i> mAffectedBlocks;
    std::unordered_set<int64_t> mAffectedKeys;
    std::vector<Vector3i> mFireIgnitions;
    std::vector<Vector3i> mSmokePositions;
    std::vector<Vector3i> mDestroyed;
    // worked out ahead by a helper thread, over blocks read for the whole tick: the rays cast and the blocks
    // they reach, the pushes given, and what is left for the tick thread
    enum class Apart : uint8_t { No, Nothing, Whole, Done };

    // worked out apart in three steps: what the rays may reach, the rays, then the actors a share at a time
    void _beginApart(const BlastField &field);

    // one slice of the rays, then what they reached put together by the tick thread
    void _castApart(const BlastField &field, int slice);

    void _markApart(BlastField &field);

    void _settleApart(const BlastField &field, const ExplosionBatch::Span &span);

    void _finishApart();

    Apart mApart = Apart::No;
    std::vector<ExplosionBatch::Hit> mApartHits;
    Vector3i mApartLow;
    Vector3i mApartHigh;
    bool mApartAny = false;
    static const int APART_SLICES = 4;
    std::vector<Reach> mApartReached[APART_SLICES];
    bool mApartMissed[APART_SLICES] = {};
    uint32_t mApartStamp = 0;
    std::optional<size_t> mQuietBlast;
    std::optional<size_t> mNear;
    // the shares of targets collected while counting, good until an actor joins or leaves
    size_t mCrowd = 0;
    uint64_t mSpansSerial = 0;
    // the blocks holding the source and every actor in reach, when they were counted one by one
    bool mHullKnown = false;
    Vector3i mHullLow;
    Vector3i mHullHigh;
    ExplosionBatch *mBatch = nullptr;
    std::unique_ptr<ExplosionBatch> mOwnBatch;
};
