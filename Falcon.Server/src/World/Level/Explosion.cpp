#include "World/Level/Explosion.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/TntBlock.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/SubChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "World/Level/Particle/BlockExplodeParticle.h"
#include "World/Level/Particle/ExplodeParticle.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <chrono>
#ifdef __APPLE__
#include <pthread/qos.h>
#include <sys/sysctl.h>
#endif
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <cmath>
#include <random>

namespace {
    const int RAYS = 16;
    const double STEP_LEN = 0.3;
    const double DEFAULT_FIRE_CHANCE = 1.0 / 3.0;
    const char *DEATH_KEY = "death.attack.explosion";
    const char *NETHER_STAR = "minecraft:nether_star";

    std::mt19937 &explosionRandom() {
        static std::mt19937 random(std::random_device{}());
        return random;
    }

    // one draw of the generator scaled to the range: an explosion asks for 1352 of them
    int32_t nextInt(int32_t min, int32_t max) {
        const uint64_t span = (uint64_t) (max - min) + 1;
        return min + (int32_t) (((uint64_t) explosionRandom()() * span) >> 32);
    }

    double nextDouble() {
        return std::uniform_real_distribution<double>(0.0, 1.0)(explosionRandom());
    }

    float resistanceOf(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data == nullptr ? 0.0f : data->mResistance;
    }

    bool isAir(const BlockState &state) {
        return state.mName == BlockNames::AIR;
    }

    AxisAlignedBB boundingBoxOf(const Vector3f &position, const ActorSize &size) {
        const float halfWidth = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - halfWidth, position.y, position.z - halfWidth, position.x + halfWidth,
                             position.y + size.mHeight, position.z + halfWidth);
    }

    AxisAlignedBB boundingBoxOf(const Vector3f &position, const std::string &identifier) {
        return boundingBoxOf(position, ActorClassRegistry::getSize(identifier));
    }

    Vector3f normalized(const Vector3f &vector) {
        const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        if (length <= 0.0f)
            return Vector3f();

        return Vector3f(vector.x / length, vector.y / length, vector.z / length);
    }

    const BlockState *airState() {
        static const BlockState *air = SubChunk::intern(BlockState());
        return air;
    }

    // rays reach 1.3 * size / 0.225 steps of 0.3, entities twice the size
    const int32_t FIELD_MARGIN = 1;
    const int32_t FIELD_MAX_RADIUS = 20;
    const int32_t FIELD_WIDE_REACH = 40;
    const size_t FIELD_WIDE_WORTH = 128;
    const size_t FIELD_WIDE_LOOKAHEAD = 1024;
    const int FIELD_WIDE_WASTES = 3;
    // cells a wide read may spend per explosion it serves, and in all: past the processor cache it is slower
    const size_t FIELD_WIDE_CELLS = 800;
    const size_t FIELD_WIDE_LIMIT = 500000;

    // blocked(x, y, z, pointX, pointY, pointZ): whether the block there holds the point
    template <typename Blocked>
    bool rayBlocked(Blocked &&blocked, double srcX, double srcY, double srcZ, double dstX, double dstY, double dstZ,
                    double stepSize) {
        const double directionX = dstX - srcX;
        const double directionY = dstY - srcY;
        const double directionZ = dstZ - srcZ;
        if (directionX == 0.0 && directionY == 0.0 && directionZ == 0.0)
            return false;

        const double length = std::sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
        const double normalX = directionX / length;
        const double normalY = directionY / length;
        const double normalZ = directionZ / length;

        for (double t = 0.0; t < length; t += stepSize) {
            const double rayX = srcX + normalX * t;
            const double rayY = srcY + normalY * t;
            const double rayZ = srcZ + normalZ * t;
            const int32_t x = (int32_t) std::floor(rayX);
            const int32_t y = (int32_t) std::floor(rayY);
            const int32_t z = (int32_t) std::floor(rayZ);

            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                continue;

            if (blocked(x, y, z, (float) rayX, (float) rayY, (float) rayZ))
                return true;
        }

        return false;
    }

    // hidden(x, y, z): whether a block stands between the source and that point of the target;
    // rowOpen(x, y): whether every point of the target at that x and y is known in sight without looking
    template <typename Hidden, typename RowOpen>
    float densityThrough(Hidden &&hidden, RowOpen &&rowOpen, const Vector3f &source,
                         const AxisAlignedBB &boundingBox) {
        if (boundingBox.isVectorInside(source))
            return 1.0f;

        const double diffX = boundingBox.mMaxX - boundingBox.mMinX;
        const double diffY = boundingBox.mMaxY - boundingBox.mMinY;
        const double diffZ = boundingBox.mMaxZ - boundingBox.mMinZ;
        const double xInterval = 1.0 / (diffX * 2.0 + 1.0);
        const double yInterval = 1.0 / (diffY * 2.0 + 1.0);
        const double zInterval = 1.0 / (diffZ * 2.0 + 1.0);

        if (xInterval < 0.0 || yInterval < 0.0 || zInterval < 0.0)
            return 0.0f;

        const double xOffset = boundingBox.mMinX + (1.0 - std::floor(1.0 / xInterval) * xInterval) / 2.0;
        const double yOffset = boundingBox.mMinY;
        const double zOffset = boundingBox.mMinZ + (1.0 - std::floor(1.0 / zInterval) * zInterval) / 2.0;

        int visibleBlocks = 0;
        int totalBlocks = 0;

        for (float x = 0.0f; x <= 1.0f; x = (float) ((double) x + xInterval)) {
            const double fromX = std::fma((double) x, diffX, xOffset);
            for (float y = 0.0f; y <= 1.0f; y = (float) ((double) y + yInterval)) {
                const double fromY = std::fma((double) y, diffY, yOffset);
                const bool open = rowOpen(fromX, fromY);
                for (float z = 0.0f; z <= 1.0f; z = (float) ((double) z + zInterval)) {
                    totalBlocks++;
                    const double fromZ = std::fma((double) z, diffZ, zOffset);

                    if (open || !hidden(fromX, fromY, fromZ))
                        visibleBlocks++;
                }
            }
        }

        return (float) visibleBlocks / (float) totalBlocks;
    }

    const size_t PARALLEL_HITS = 512;
    // with the helpers kept awake for the tick, a smaller crowd is already worth sharing out
    const size_t CROWDED_PARALLEL_HITS = 64;
    // explosions that break nothing: from that many on they are shared out to every core, a few at a time
    const size_t QUIET_CROWD = 16;
    const size_t QUIET_SHARE = 4;
    // explosions that break blocks: two this many blocks apart on an axis neither read what the other
    // breaks nor push the same actors; and how far from the middle of the lot the one read of blocks goes
    const int32_t APART_BLOCKS = 18;
    const int32_t APART_SPAN = 56;
    // explosions worked out together in one round, and how far ahead a round looks for them
    const size_t APART_ROUND = 12;
    const size_t APART_LOOKAHEAD = 192;
    // fewer shares of actors than this are settled by the tick thread alone
    const size_t APART_SHARES_ALONE = 6;
    const size_t APART_HANDFUL = 8;
    // shares of actors settled between two makings of the table of clear boxes: at least that many, and
    // one for that many blocks read
    const size_t APART_SUM_SHARES = 8192;
    const size_t APART_SUM_CELLS = 16;
    // room for the rounding of the slice bounds: a wider slice only asks about more blocks
    const double CONE_SLACK = 1.0e-4;
    // past that many candidates around, they are not counted one by one before the work
    const size_t COUNTED_CROWD = 2048;
    // up to that many, walking every ray costs less than the table that tells the free ones
    const size_t FEW_TARGETS = 12;

    // a few threads kept for the walked rays of a crowded explosion; they spin between two explosions of a tick
    class BlastWorkers {
    public:
        static BlastWorkers &get() {
            static BlastWorkers workers;
            return workers;
        }

        template <typename Job>
        void run(size_t count, Job &&job) {
            std::function<void(size_t)> call = [&job](size_t index) { job(index); };
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mJob = &call;
                mCount = count;
                mNext.store(0, std::memory_order_relaxed);
                mPending.store(count, std::memory_order_relaxed);
                mRound.fetch_add(1, std::memory_order_release);
            }
            // the helpers still spinning from the last explosion see the round change by themselves
            if (mAsleep.load(std::memory_order_acquire) != 0)
                mWake.notify_all();
            _work(call, count);
            // the last shares end within microseconds: giving the core away here costs a whole time slice
            const std::chrono::steady_clock::time_point ownEnd = std::chrono::steady_clock::now();
            _await([this] { return mPending.load(std::memory_order_acquire) == 0; });
            {
                // no helper picks the job up past this point; the ones inside are waited for
                std::lock_guard<std::mutex> lock(mMutex);
                mJob = nullptr;
            }
            _await([this] { return mBusy.load(std::memory_order_acquire) == 0; });

            // a helper that shares its core with another program finishes after this thread, which waits:
            // the wait is watched, and one helper less is asked when it grows
            const std::chrono::nanoseconds waited = std::chrono::steady_clock::now() - ownEnd;
            mWaited += waited;
            ++mJobs;
            if (waited > STALL && ++mStalls >= STALLS_ALLOWED) {
                mStalls = 0;
                mStalled = true;
                _setAside();
            }
        }

        // while hot, the helpers stay awake between two explosions
        void heat(bool hot) {
            if (hot && mHot.load(std::memory_order_relaxed) == 0) {
                // judged on the last tick of explosions: long waits on average set a helper aside, short
                // ones and no stall give one back
                const std::chrono::nanoseconds average = mJobs == 0 ? std::chrono::nanoseconds(0) : mWaited / mJobs;
                const unsigned active = mActive.load(std::memory_order_relaxed);
                if (average > SLOW_WAIT)
                    _setAside();
                else if (!mStalled && mJobs != 0 && average < FAST_WAIT && active < mThreads.size())
                    mActive.store(active + 1, std::memory_order_relaxed);
                mStalled = false;
                mStalls = 0;
                mWaited = std::chrono::nanoseconds(0);
                mJobs = 0;
            }
            mHot.fetch_add(hot ? 1 : -1, std::memory_order_relaxed);
        }

        ~BlastWorkers() {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStop = true;
                mRound.fetch_add(1, std::memory_order_release);
            }
            mWake.notify_all();
            for (std::thread &thread: mThreads)
                thread.join();
        }

    private:
        BlastWorkers() {
            const unsigned cores = std::thread::hardware_concurrency();
            // one helper a fast core, the calling thread on the last: more of them, or slow cores, wait on
            // one another longer than they work
            unsigned helpers = cores > 1 ? std::min(cores / 2, 4u) - 1 : 0;
#ifdef __APPLE__
            int fast = 0;
            size_t length = sizeof(fast);
            if (sysctlbyname("hw.perflevel0.physicalcpu", &fast, &length, nullptr, 0) == 0 && fast > 1)
                helpers = (unsigned) fast - 1;
#endif
            mActive.store(helpers, std::memory_order_relaxed);
            for (unsigned index = 0; index < helpers; ++index)
                mThreads.emplace_back([this, index] {
#ifdef __APPLE__
                    // a share held by a thread pushed off its core stalls the tick: these rank with the
                    // threads a user is waiting on, above background work
                    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
                    _loop(index);
                });
        }

        void _setAside() {
            const unsigned active = mActive.load(std::memory_order_relaxed);
            if (active > 0)
                mActive.store(active - 1, std::memory_order_relaxed);
        }

        template <typename Done>
        static void _await(Done &&done) {
            for (int spins = 0; !done(); ++spins) {
                // a helper that lost its core for long is not waited for at full speed
                if (spins > AWAIT_SPINS)
                    std::this_thread::yield();
            }
        }

        void _work(const std::function<void(size_t)> &call, size_t count) {
            for (;;) {
                const size_t first = mNext.fetch_add(CHUNK, std::memory_order_relaxed);
                if (first >= count)
                    return;
                const size_t last = std::min(count, first + CHUNK);
                for (size_t index = first; index < last; ++index)
                    call(index);
                mPending.fetch_sub(last - first, std::memory_order_release);
            }
        }

        void _loop(unsigned index) {
            uint64_t seen = 0;
            for (;;) {
                // spin a moment: the next explosion of the tick comes right after; all along a tick of many
                // explosions, where going to sleep between two of them costs more than the work itself
                int spins = 0;
                while (mRound.load(std::memory_order_acquire) == seen
                       && (spins < SPINS || mHot.load(std::memory_order_relaxed) != 0))
                    ++spins;
                if (mRound.load(std::memory_order_acquire) == seen) {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mAsleep.fetch_add(1, std::memory_order_release);
                    mWake.wait(lock, [&] { return mRound.load(std::memory_order_acquire) != seen; });
                    mAsleep.fetch_sub(1, std::memory_order_release);
                }

                const std::function<void(size_t)> *call = nullptr;
                size_t count = 0;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mStop)
                        return;
                    seen = mRound.load(std::memory_order_acquire);
                    // a helper set aside lets the round pass
                    call = index < mActive.load(std::memory_order_relaxed) ? mJob : nullptr;
                    count = mCount;
                    if (call != nullptr)
                        mBusy.fetch_add(1, std::memory_order_release);
                }
                if (call == nullptr)
                    continue;
                _work(*call, count);
                mBusy.fetch_sub(1, std::memory_order_release);
            }
        }

        static const size_t CHUNK = 1;
        static const int AWAIT_SPINS = 200000;
        static const int SPINS = 20000;
        static constexpr std::chrono::microseconds STALL{400};
        static const int STALLS_ALLOWED = 2;
        static constexpr std::chrono::microseconds SLOW_WAIT{40};
        static constexpr std::chrono::microseconds FAST_WAIT{12};

        std::vector<std::thread> mThreads;
        std::mutex mMutex;
        std::condition_variable mWake;
        const std::function<void(size_t)> *mJob = nullptr;
        size_t mCount = 0;
        std::atomic<size_t> mNext{0};
        std::atomic<size_t> mPending{0};
        std::atomic<int> mBusy{0};
        std::atomic<int> mAsleep{0};
        std::atomic<int> mHot{0};
        std::atomic<unsigned> mActive{0};
        // touched by the calling thread alone
        int mStalls = 0;
        bool mStalled = false;
        std::chrono::nanoseconds mWaited{0};
        std::chrono::nanoseconds::rep mJobs = 0;
        std::atomic<uint64_t> mRound{0};
        bool mStop = false;
    };

    // cheap and never wrong about a target in reach: the exact distance is asked only past it
    bool maybeWithin(const Vector3f &left, const Vector3f &right, double reach) {
        const double x = (double) left.x - (double) right.x;
        const double y = (double) left.y - (double) right.y;
        const double z = (double) left.z - (double) right.z;
        return x * x + y * y + z * z <= reach * reach * 1.000001;
    }

    double distanceBetween(const Vector3f &left, const Vector3f &right) {
        const double x = (double) left.x - (double) right.x;
        const double y = (double) left.y - (double) right.y;
        const double z = (double) left.z - (double) right.z;
        return std::sqrt(x * x + y * y + z * z);
    }
}

Explosion::Explosion(ServerNetworkHandler &owner, Level &level, const Vector3f &center, double size,
                     const ServerActor *sourceActor, bool fromTnt)
        : mOwner(owner), mLevel(level), mSource(center), mSize(std::max(size, 0.0)), mSourceActor(sourceActor),
          mFromTnt(fromTnt) {}

void Explosion::setIncendiary(bool incendiary) {
    if (!incendiary)
        mFireChance = 0.0;
    else if (mFireChance <= 0.0)
        mFireChance = DEFAULT_FIRE_CHANCE;
}

int64_t Explosion::_key(const Vector3i &position) {
    return (((int64_t) position.x & 0x3FFFFFF) << 38) | (((int64_t) position.z & 0x3FFFFFF) << 12)
           | ((int64_t) (position.y + 2048) & 0xFFF);
}

bool Explosion::explode() {
    if (explodeA())
        return explodeB();

    return false;
}

bool Explosion::explodeWithoutBlocks() {
    if (!_allowByPlugins())
        return false;

    return explodeB();
}

bool Explosion::explodeA() {
    if (!_findAffectedBlocks())
        return false;

    return _allowByPlugins();
}

bool Explosion::_allowByPlugins() {
    PluginManager &plugins = mOwner.getPluginManager();
    if (!plugins.hasSubscribers(FALCON_EVENT_EXPLOSION))
        return _allowByScripts();

    PluginEvent explosionEvent;
    explosionEvent.mType = FALCON_EVENT_EXPLOSION;
    explosionEvent.mCancellable = true;
    explosionEvent.mEntity = const_cast<ServerActor *>(mSourceActor);
    explosionEvent.mLevel = &mLevel;
    explosionEvent.mPosition = mSource;
    explosionEvent.mAmount = mSize;
    explosionEvent.mBlocks = &mAffectedBlocks;
    plugins.dispatch(explosionEvent);
    if (explosionEvent.mCancelled)
        return false;

    mSize = std::max(explosionEvent.mAmount, 0.0);
    return _allowByScripts();
}

bool Explosion::_allowByScripts() {
    std::vector<Vector3i> impacted;
    for (const Vector3i &position: mAffectedBlocks) {
        const BlockState *state = mLevel.peekBlockPtr(position.x, position.y, position.z);
        if (state != nullptr && !isAir(*state))
            impacted.push_back(position);
    }
    if (mOwner.getScriptEngine().beforeExplosion(mLevel, mSourceActor, impacted))
        return false;
    mAffectedBlocks = impacted;
    mAffectedKeys.clear();
    return true;
}

bool Explosion::breaksNothingAt(Level &level, const Vector3f &center) {
    const int32_t x = (int32_t) std::floor(center.x);
    const int32_t y = (int32_t) std::floor(center.y);
    const int32_t z = (int32_t) std::floor(center.z);
    // a tnt thrown past the loaded chunks breaks nothing there: no chunk is made for it
    const BlockState *layer0 = level.peekBlockPtr(x, y, z);
    if (layer0 == nullptr)
        return y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y;
    if (LiquidView(*layer0).isWater())
        return true;

    const BlockState *layer1 = level.peekBlockPtr(x, y, z, 1);
    return layer1 != nullptr && LiquidView(*layer1).isWater();
}

bool Explosion::isReachEmpty(Level &level, const Vector3f &center, double size) {
    const int32_t reach = BlastField::rayReach(size);
    const int32_t x = (int32_t) std::floor(center.x);
    const int32_t y = (int32_t) std::floor(center.y);
    const int32_t z = (int32_t) std::floor(center.z);
    const int32_t bottom = std::max(y - reach, LevelChunk::MIN_Y);
    const int32_t top = std::min(y + reach, LevelChunk::MAX_Y);
    if (bottom > top)
        return true;

    for (int32_t chunkX = (x - reach) >> 4; chunkX <= (x + reach) >> 4; ++chunkX) {
        for (int32_t chunkZ = (z - reach) >> 4; chunkZ <= (z + reach) >> 4; ++chunkZ) {
            const LevelChunk *chunk = level.peekChunkPtr(chunkX, chunkZ);
            if (chunk == nullptr)
                continue;
            for (int section = LevelChunk::subChunkIndexFor(bottom); section <= LevelChunk::subChunkIndexFor(top);
                 ++section) {
                if (!chunk->getSubChunk(section).isEmpty())
                    return false;
            }
        }
    }
    return true;
}

bool Explosion::_findAffectedBlocks() {
    // an explosion in water still hurts entities, breaks no blocks
    if (mInWater.has_value()) {
        if (*mInWater)
            return true;
    } else if (mSourceActor != nullptr && breaksNothingAt(mLevel, mSource)) {
        return true;
    }

    if (mSize < 0.1)
        return false;

    // in the open sky no ray meets a block: asked of whole sections, without reading any block
    if (isReachEmpty(mLevel, mSource, mSize))
        return true;

    BlastField &field = _field(FieldUse::Rays);
    // nothing but air in reach: no ray meets a block
    Vector3i blocksLow;
    Vector3i blocksHigh;
    if (!field.findBlocksInReach(blocksLow, blocksHigh))
        return true;

    field.beginRays();

    // cast by the helper threads when the tick is full of explosions, then gathered in order: a block
    // comes up once, from the first ray that reached it
    std::vector<std::vector<Reach>> &slices = _batch().getRaySlices();
    slices.resize(RAYS);
    for (std::vector<Reach> &slice: slices)
        slice.clear();
    bool cast = false;
    if (_batch().isCrowded()) {
        std::atomic<bool> whole{true};
        const BlastField &read = field;
        BlastWorkers::get().run((size_t) RAYS, [&](size_t slice) {
            if (!_castRays(read, (int) slice, (int) slice + 1, blocksLow, blocksHigh, true, slices[slice]))
                whole.store(false, std::memory_order_relaxed);
        });
        cast = whole.load(std::memory_order_relaxed);
        if (!cast) {
            for (std::vector<Reach> &slice: slices)
                slice.clear();
        }
    }
    if (!cast)
        _castRays(field, 0, RAYS, blocksLow, blocksHigh, false, slices[0]);

    const bool incendiary = mFireChance > 0.0;
    for (const std::vector<Reach> &slice: slices) {
        for (const Reach &reach: slice) {
            if (!(reach.mIndex >= 0 ? field.markReached(reach.mIndex) : mAffectedKeys.insert(_key(reach.mBlock)).second))
                continue;
            mAffectedBlocks.push_back(reach.mBlock);
            if (incendiary && nextDouble() <= mFireChance)
                mFireIgnitions.push_back(reach.mBlock);
        }
    }

    return true;
}

bool Explosion::_castRays(const BlastField &field, int from, int to, const Vector3i &blocksLow,
                          const Vector3i &blocksHigh, bool helper, std::vector<Reach> &reached) const {
    // each helper thread draws from its own generator
    thread_local std::mt19937 helperRandom(std::random_device{}());
    std::mt19937 &random = helper ? helperRandom : explosionRandom();
    // the farthest a ray gets, in steps
    const double farthest = std::ceil(mSize * 1.3 / (STEP_LEN * 0.75)) - 1.0;
    const int lastRay = RAYS - 1;
    static const std::vector<double> strideTable = [] {
        std::vector<double> table((size_t) (RAYS * RAYS * RAYS) * 3);
        const int last = RAYS - 1;
        for (int i = 0; i < RAYS; ++i) {
            for (int j = 0; j < RAYS; ++j) {
                for (int k = 0; k < RAYS; ++k) {
                    double x = (double) i / (double) last * 2.0 - 1.0;
                    double y = (double) j / (double) last * 2.0 - 1.0;
                    double z = (double) k / (double) last * 2.0 - 1.0;
                    const double length = std::sqrt(x * x + y * y + z * z);
                    double *stride = table.data() + (size_t) ((i * RAYS + j) * RAYS + k) * 3;
                    stride[0] = x / length * STEP_LEN;
                    stride[1] = y / length * STEP_LEN;
                    stride[2] = z / length * STEP_LEN;
                }
            }
        }
        return table;
    }();
    const double *strides = strideTable.data();

    // one ray per surface point of a 16x16x16 cube; force 0.7-1.3x the size, lost with distance and the
    // blast resistance of every block crossed
    for (int i = from; i < to; ++i) {
        for (int j = 0; j < RAYS; ++j) {
            for (int k = 0; k < RAYS; ++k) {
                if (i != 0 && i != lastRay && j != 0 && j != lastRay && k != 0 && k != lastRay)
                    continue;

                // the step of each ray is the same for every explosion: worked out once
                const double *stride = strides + (size_t) ((i * RAYS + j) * RAYS + k) * 3;
                const double vectorX = stride[0];
                const double vectorY = stride[1];
                const double vectorZ = stride[2];

                double pointerX = mSource.x;
                double pointerY = mSource.y;
                double pointerZ = mSource.z;

                // a ray that stays clear of the box holding the blocks breaks nothing: not walked
                const double endX = pointerX + vectorX * farthest;
                const double endY = pointerY + vectorY * farthest;
                const double endZ = pointerZ + vectorZ * farthest;
                if (std::floor(std::max(pointerX, endX)) + 1.0 < (double) blocksLow.x
                    || std::floor(std::min(pointerX, endX)) - 1.0 > (double) blocksHigh.x
                    || std::floor(std::max(pointerY, endY)) + 1.0 < (double) blocksLow.y
                    || std::floor(std::min(pointerY, endY)) - 1.0 > (double) blocksHigh.y
                    || std::floor(std::max(pointerZ, endZ)) + 1.0 < (double) blocksLow.z
                    || std::floor(std::min(pointerZ, endZ)) - 1.0 > (double) blocksHigh.z)
                    continue;

                // past the box holding the blocks and heading away, a ray meets nothing more: the steps it
                // has left inside, with one to spare for the rounding of the walk
                double inside = farthest + 2.0;
                const double pointers[3] = {pointerX, pointerY, pointerZ};
                const double vectors[3] = {vectorX, vectorY, vectorZ};
                const int32_t lows[3] = {blocksLow.x, blocksLow.y, blocksLow.z};
                const int32_t highs[3] = {blocksHigh.x, blocksHigh.y, blocksHigh.z};
                for (int axis = 0; axis < 3; ++axis) {
                    if (vectors[axis] > 0.0)
                        inside = std::min(inside,
                                          std::ceil(((double) highs[axis] + 1.0 - pointers[axis]) / vectors[axis]) + 1.0);
                    else if (vectors[axis] < 0.0)
                        inside = std::min(inside,
                                          std::floor((pointers[axis] - (double) lows[axis]) / -vectors[axis]) + 2.0);
                }
                if (inside <= 0.0)
                    continue;

                const uint64_t draw = (uint64_t) random() * 601;
                double step = 0.0;
                for (double blastForce = mSize * (double) (700 + (int32_t) (draw >> 32)) / 1000.0;
                     blastForce > 0.0 && step < inside; blastForce -= STEP_LEN * 0.75, step += 1.0) {
                    const Vector3i block((int32_t) std::floor(pointerX), (int32_t) std::floor(pointerY),
                                         (int32_t) std::floor(pointerZ));
                    if (block.y < LevelChunk::MIN_Y || block.y > LevelChunk::MAX_Y)
                        break;

                    float resistance = -1.0f;
                    const int32_t index = field.indexOf(block.x, block.y, block.z);
                    if (index >= 0) {
                        if (field.stateAt(index) != nullptr)
                            resistance = field.resistanceAt(index);
                    } else if (helper) {
                        // past the blocks read: the level is the tick thread's to ask
                        return false;
                    } else {
                        const BlockState *layer0 = mLevel.peekBlockPtr(block.x, block.y, block.z);
                        if (layer0 != nullptr && !isAir(*layer0) && resistanceOf(*layer0) != -1.0f) {
                            const BlockState *layer1 = mLevel.peekBlockPtr(block.x, block.y, block.z, 1);
                            resistance = std::max(resistanceOf(*layer0),
                                                  layer1 == nullptr ? 0.0f : resistanceOf(*layer1));
                        }
                    }

                    if (resistance != -1.0f) {
                        blastForce -= ((double) resistance / 5.0 + 0.3) * STEP_LEN;
                        if (blastForce > 0.0)
                            reached.push_back(Reach{index, block});
                    }

                    pointerX += vectorX;
                    pointerY += vectorY;
                    pointerZ += vectorZ;
                }
            }
        }
    }
    return true;
}

namespace {
    float quickDensityOf(const BlastField &field, const Vector3f &source, const AxisAlignedBB &boundingBox);

    float walkedDensityOf(const BlastField &field, const Vector3f &source, const AxisAlignedBB &boundingBox);

    // one for every explosion worked out by a helper, never handed out twice
    std::atomic<uint32_t> gRayStamps{1};

    // actors resting on one spot share one answer: the same box seen from the same source through the same blocks
    struct DensityMemo {
        struct Slot {
            uint32_t mStamp = 0;
            Vector3f mPosition;
            float mWidth = 0.0f;
            float mHeight = 0.0f;
            float mDensity = 0.0f;
        };
        static const size_t SLOTS = 1024;
        Slot mSlots[SLOTS];
    };

    template<typename Compute>
    float rememberedDensity(uint32_t stamp, const ExplosionBatch::Target &target, Compute &&compute) {
        thread_local DensityMemo memo;
        uint32_t bits[3];
        std::memcpy(bits, &target.mPosition, sizeof(bits));
        const uint32_t hash = (bits[0] * 0x9E3779B1u) ^ (bits[1] * 0x85EBCA77u) ^ (bits[2] * 0xC2B2AE3Du);
        DensityMemo::Slot &slot = memo.mSlots[(hash >> 16) % DensityMemo::SLOTS];
        if (slot.mStamp == stamp && slot.mPosition.x == target.mPosition.x && slot.mPosition.y == target.mPosition.y
            && slot.mPosition.z == target.mPosition.z && slot.mWidth == target.mWidth
            && slot.mHeight == target.mHeight)
            return slot.mDensity;

        slot.mStamp = stamp;
        slot.mPosition = target.mPosition;
        slot.mWidth = target.mWidth;
        slot.mHeight = target.mHeight;
        slot.mDensity = compute();
        return slot.mDensity;
    }
}

namespace {
    // the rare target that is not settled by a helper
    std::mutex gApartHitsMutex;
}

void Explosion::_beginApart(const BlastField &field) {
    mApartHits.clear();
    mAffectedBlocks.clear();
    mApartAny = false;
    if (mSize < 0.1) {
        mApart = Apart::Nothing;
        return;
    }

    const Vector3i center((int32_t) std::floor(mSource.x), (int32_t) std::floor(mSource.y),
                          (int32_t) std::floor(mSource.z));
    // past the blocks read for the tick: the tick thread does this one whole, from the level
    if (!field.scanBlocks(center, BlastField::rayReach(mSize), mApartLow, mApartHigh, mApartAny)) {
        mApart = Apart::Whole;
        return;
    }
    mApartStamp = gRayStamps.fetch_add(1, std::memory_order_relaxed);
    mApart = Apart::Done;
}

void Explosion::_castApart(const BlastField &field, int slice) {
    std::vector<Reach> &reached = mApartReached[slice];
    reached.clear();
    const int width = RAYS / APART_SLICES;
    mApartMissed[slice] = !_castRays(field, slice * width, (slice + 1) * width, mApartLow, mApartHigh, true, reached);
}

void Explosion::_markApart(BlastField &field) {
    for (const bool missed: mApartMissed) {
        // a ray left the blocks read: the rays alone are cast again here, from the level; the actors
        // were settled all the same
        if (missed) {
            mAffectedBlocks.clear();
            mFireIgnitions.clear();
            _findAffectedBlocks();
            return;
        }
    }
    // no other explosion of the round is close enough to mark the same blocks
    const uint32_t stamp = gRayStamps.fetch_add(1, std::memory_order_relaxed);
    const bool incendiary = mFireChance > 0.0;
    for (const std::vector<Reach> &reached: mApartReached) {
        for (const Reach &reach: reached) {
            if (!field.markReachedWith(reach.mIndex, stamp))
                continue;
            mAffectedBlocks.push_back(reach.mBlock);
            if (incendiary && nextDouble() <= mFireChance)
                mFireIgnitions.push_back(reach.mBlock);
        }
    }
}

void Explosion::_settleApart(const BlastField &field, const ExplosionBatch::Span &span) {
    const double reach = mSize * 2.0;
    for (size_t offset = 0; offset < span.mCount; ++offset) {
        const ExplosionBatch::Target &target = span.mFirst[offset];
        if (target.mActor == mSourceActor || !maybeWithin(target.mPosition, mSource, reach))
            continue;
        const double distance = distanceBetween(target.mPosition, mSource) / reach;
        if (distance > 1.0)
            continue;
        // gone at the end of the tick, and nothing hurts it: the push would move nothing
        if (target.mInvulnerable && target.mActor->isExpired())
            continue;

        ActorSize size;
        size.mWidth = target.mWidth;
        size.mHeight = target.mHeight;
        const AxisAlignedBB box = boundingBoxOf(target.mPosition, size);
        const float density = rememberedDensity(mApartStamp, target, [&] {
            const float quick = quickDensityOf(field, mSource, box);
            return quick < 0.0f ? walkedDensityOf(field, mSource, box) : quick;
        });

        // an invulnerable target takes the push alone: no other explosion at work reaches it
        if (density >= 0.0f && target.mInvulnerable) {
            const Vector3f &position = target.mPosition;
            const Vector3f motion = normalized(
                    Vector3f(position.x - mSource.x, position.y - mSource.y, position.z - mSource.z));
            const double impact = (1.0 - distance) * density;
            const Vector3f current = target.mActor->getMotion();
            target.mActor->setMotion(Vector3f(current.x + motion.x * (float) impact,
                                              current.y + motion.y * (float) impact,
                                              current.z + motion.z * (float) impact));
            continue;
        }
        std::lock_guard<std::mutex> lock(gApartHitsMutex);
        mApartHits.push_back(ExplosionBatch::Hit{&target, distance, density});
    }
}

void Explosion::_finishApart() {
    if (mApart == Apart::Nothing)
        return;
    if (mApart != Apart::Done) {
        mApart = Apart::No;
        explode();
        return;
    }

    // the say of plugins and scripts, then the damage, the blocks and the effects, as any explosion ends
    if (_allowByPlugins())
        explodeB();
}

bool Explosion::explodeB() {
    _damageEntities();
    _destroyBlocks();
    _ignite();
    _playEffects();
    mOwner.getScriptEngine().onExplosion(mLevel, mSourceActor, mDestroyed);
    return true;
}

float Explosion::_calculateEntityDamage(double doubleRadius, double impact) {
    return (float) ((impact * impact + impact) / 2.0 * 7.0 * doubleRadius + 1.0);
}

float Explosion::_scaleDamageForDifficulty(float damage) const {
    switch (mOwner.getProperties().getDifficulty()) {
        case Difficulty::Peaceful:
            return 0.0f;
        case Difficulty::Easy:
            return std::min(damage / 2.0f + 1.0f, damage);
        case Difficulty::Hard:
            return damage * 1.5f;
        default:
            return damage;
    }
}

void Explosion::_damageEntities() {
    const double explosionSize = mSize * 2.0;

    for (auto &entry: mOwner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || &mOwner.getLevelFor(player) != &mLevel)
            continue;

        const double distance = distanceBetween(player.getPosition(), mSource) / explosionSize;
        if (distance > 1.0)
            continue;

        const Vector3f position = player.getPosition();
        const Vector3f motion = normalized(
                Vector3f(position.x - mSource.x, position.y - mSource.y, position.z - mSource.z));
        const float density = _density(boundingBoxOf(position, "minecraft:player"));
        const double impact = (1.0 - distance) * density;
        const float damage = _scaleDamageForDifficulty(_calculateEntityDamage(explosionSize, impact));

        mOwner.hurt(player, damage, ActorDamageSource::environment(DEATH_KEY, player.getName()).fromOrigin(mSource));

        // pushes the replayed velocity, not the last reported delta; tick-stamped so both sides take it on the same input
        const Vector3f current = player.hasSimulatedVelocity() ? player.getSimulatedVelocity() : player.getMotion();
        player.applyServerMotion(mOwner,
                                 Vector3f(current.x + motion.x * (float) impact, current.y + motion.y * (float) impact,
                                          current.z + motion.z * (float) impact));
    }

    ExplosionBatch &batch = _batch();
    const ActorSize itemSize = ActorClassRegistry::getSize("minecraft:item");

    using Hit = ExplosionBatch::Hit;
    using Target = ExplosionBatch::Target;
    std::vector<Hit> &hits = batch.getHits();
    std::vector<ExplosionBatch::Span> &spans = batch.getSpans();
    hits.clear();
    const bool settled = mApart == Apart::Done || (mQuietBlast.has_value() && batch.isSettled(*mQuietBlast));
    if (mApart == Apart::Done)
        hits.swap(mApartHits);
    else if (settled)
        hits.swap(batch.getQuietHits(*mQuietBlast));
    const bool counted = mNear.has_value();
    const size_t nearby = settled ? 0 : _near();
    // the shares collected for the count still stand unless the rays made or removed an actor
    const size_t crowd = settled ? 0
                         : !counted || mSpansSerial == mOwner.getActorSetSerial()
                                   ? mCrowd
                                   : batch.collectSpans(mLevel, mSource, (float) explosionSize, spans);
    if (nearby == 0)
        spans.clear();
    else
        _field(FieldUse::Targets, nearby > FEW_TARGETS);

    const auto push = [this](const Target &target, double distance, float density) {
        const Vector3f &position = target.mPosition;
        const Vector3f motion = normalized(
                Vector3f(position.x - mSource.x, position.y - mSource.y, position.z - mSource.z));
        const double impact = (1.0 - distance) * density;
        const Vector3f current = target.mActor->getMotion();
        target.mActor->setMotion(Vector3f(current.x + motion.x * (float) impact,
                                          current.y + motion.y * (float) impact,
                                          current.z + motion.z * (float) impact));
        return impact;
    };

    // what only reads the blocks and moves one actor runs on every core when the crowd is large: an
    // invulnerable target takes the push alone, nothing else of the world is touched
    std::mutex hitsMutex;
    const uint32_t memoStamp = gRayStamps.fetch_add(1, std::memory_order_relaxed);
    const auto settle = [&](size_t index) {
        const ExplosionBatch::Span &span = spans[index];
        for (size_t offset = 0; offset < span.mCount; ++offset) {
            const Target &target = span.mFirst[offset];
            ServerActor *actor = target.mActor;
            if (actor == mSourceActor || !maybeWithin(target.mPosition, mSource, explosionSize))
                continue;

            const double distance = distanceBetween(target.mPosition, mSource) / explosionSize;
            if (distance > 1.0)
                continue;
            // gone at the end of the tick, and nothing hurts it: the push would move nothing
            if (target.mInvulnerable && actor->isExpired())
                continue;

            ActorSize size;
            size.mWidth = target.mWidth;
            size.mHeight = target.mHeight;
            const AxisAlignedBB box = boundingBoxOf(target.mPosition, size);
            const float density = rememberedDensity(memoStamp, target, [&] {
                const float quick = _quickDensity(box);
                return quick < 0.0f ? _walkedDensity(box) : quick;
            });

            if (density >= 0.0f && target.mInvulnerable) {
                push(target, distance, density);
                continue;
            }
            std::lock_guard<std::mutex> lock(hitsMutex);
            hits.push_back(Hit{&target, distance, density});
        }
    };
    if (nearby >= (batch.isCrowded() ? CROWDED_PARALLEL_HITS : PARALLEL_HITS))
        BlastWorkers::get().run(spans.size(), settle);
    else
        for (size_t index = 0; index < spans.size(); ++index)
            settle(index);

    for (const Hit &hit: hits) {
        const Target &target = *hit.mTarget;
        ServerActor *actor = target.mActor;
        if (!target.mInvulnerable && !actor->isAlive())
            continue;

        ActorSize size;
        size.mWidth = target.mWidth;
        size.mHeight = target.mHeight;
        const float density = hit.mDensity >= 0.0f ? hit.mDensity
                                                    : _density(boundingBoxOf(target.mPosition, size));
        const double impact = (1.0 - hit.mDistance) * density;
        if (!target.mInvulnerable)
            mOwner.damageActor(*actor, _calculateEntityDamage(explosionSize, impact),
                               ActorDamageSource::environment(DEATH_KEY, actor->getName())
                                       .withDamager(const_cast<ServerActor *>(mSourceActor)));
        push(target, hit.mDistance, density);
    }
    batch.forEachItem(mLevel, mSource, (float) explosionSize, [&](ItemActor *item) {
        if (item->isRemoved())
            return;

        const Vector3f position = item->getPosition();
        const double distance = distanceBetween(position, mSource) / explosionSize;
        if (distance > 1.0 || _isInsideWater(position))
            return;

        const ItemStack &stack = item->getItem();
        if (stack.mDefinition != nullptr && stack.mDefinition->getIdentifier() == NETHER_STAR)
            return;

        const float density = _density(boundingBoxOf(position, itemSize));
        const double impact = (1.0 - distance) * density;
        if (item->reduceHealth(_calculateEntityDamage(explosionSize, impact)) <= 0.0f)
            item->setRemoved(true);
    });
}

bool Explosion::_isInsideWater(const Vector3f &position) const {
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return false;

    const BlockState *layer0 = mLevel.peekBlockPtr(x, y, z);
    if (layer0 == nullptr)
        return false;
    if (LiquidView(*layer0).isWater())
        return true;

    const BlockState *layer1 = mLevel.peekBlockPtr(x, y, z, 1);
    return layer1 != nullptr && LiquidView(*layer1).isWater();
}

void Explosion::_destroyBlocks() {
    const double yield = mFromTnt && !mLevel.getGameRules().getBool("tntexplosiondropdecay") ? 100.0
                                                                                             : (1.0 / mSize) * 100.0;
    const ItemStack noTool;

    for (const Vector3i &position: mAffectedBlocks) {
        const BlockState *inPlace = mLevel.peekBlockPtr(position.x, position.y, position.z);
        if (inPlace == nullptr || isAir(*inPlace))
            continue;
        // the state the chunk holds lives as long as the process: no copy is made of it
        const BlockState &state = *inPlace;
        // what its name tells, asked once per state
        enum : uint8_t { TNT = 1, KEEPS_CONTENTS = 2 };
        thread_local std::unordered_map<const BlockState *, uint8_t> kinds;
        auto kind = kinds.find(inPlace);
        if (kind == kinds.end()) {
            const ContainerBlockDefinition *container = ContainerBlock::findDefinition(state.mName);
            const bool keeps = container != nullptr && container->mKind == ContainerBlockKind::ShulkerBox;
            kind = kinds.emplace(inPlace, (uint8_t) ((TntBlock::matches(state.mName) ? TNT : 0)
                                                     | (keeps ? KEEPS_CONTENTS : 0)))
                           .first;
        }

        if (nextInt(0, 7) == 0)
            mSmokePositions.push_back(position);

        if ((kind->second & TNT) != 0) {
            TntBlock::prime(mOwner, mLevel, position, nextInt(10, 30));
            continue;
        }

        const BlockState *layerOne = mLevel.peekBlockPtr(position.x, position.y, position.z, 1);
        if (layerOne != nullptr && !isAir(*layerOne))
            mLevel.setBlockStateAtLayer(position.x, position.y, position.z, 1, BlockState());

        const bool keepsContents = (kind->second & KEEPS_CONTENTS) != 0;
        BlockActionHandler::destroyBlock(mOwner, mLevel, position, state, keepsContents || nextDouble() * 100.0 < yield,
                                         noTool, mSourceActor);
        mDestroyed.push_back(position);
        mOwner.getScriptEngine().onBlockExplode(mLevel, position, state, mSourceActor);
    }
}

void Explosion::_ignite() {
    for (const Vector3i &position: mFireIgnitions) {
        const BlockState state = mLevel.getBlockState(position.x, position.y, position.z);
        const BlockData *below = BlockDataTable::find(
                mLevel.getBlockState(position.x, position.y - 1, position.z).mName.c_str());
        if (!isAir(state) || below == nullptr || !below->mSolid)
            continue;

        Tag states = Tag::ofCompound();
        states.putInt("age", 0);
        mLevel.setBlock(position, BlockState("minecraft:fire", states), false);
    }
}

void Explosion::_playEffects() {
    mOwner.playLevelSound(mLevel, LevelSoundEvent::EXPLODE, mSource);

    mLevel.addParticle(ExplodeParticle(mSource, mSize));
    mLevel.addParticle(BlockExplodeParticle(mSource, mSize, mSmokePositions));
}

float Explosion::getBlockDensity(Level &level, const Vector3f &source, const AxisAlignedBB &boundingBox) {
    return densityThrough(
            [&level, &source](double x, double y, double z) {
                return isRayCollidingWithBlocks(level, source.x, source.y, source.z, x, y, z, 0.3);
            },
            [](double, double) { return false; }, source, boundingBox);
}

bool Explosion::isRayCollidingWithBlocks(Level &level, double srcX, double srcY, double srcZ, double dstX, double dstY,
                                         double dstZ, double stepSize) {
    return rayBlocked(
            [&level](int32_t x, int32_t y, int32_t z, float pointX, float pointY, float pointZ) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                return state != nullptr && BlockShape::isPositionInside(*state, x, y, z, pointX, pointY, pointZ);
            },
            srcX, srcY, srcZ, dstX, dstY, dstZ, stepSize);
}

void ExplosionBatch::setCrowded(bool crowded) {
    if (crowded == mCrowded)
        return;
    mCrowded = crowded;
    BlastWorkers::get().heat(crowded);
}

ExplosionBatch::~ExplosionBatch() {
    setCrowded(false);
}

ExplosionBatch &Explosion::_batch() {
    if (mBatch == nullptr) {
        mOwnBatch.reset(new ExplosionBatch(mOwner));
        mBatch = mOwnBatch.get();
    }
    return *mBatch;
}

size_t Explosion::_near() {
    if (mNear.has_value())
        return *mNear;

    const double explosionSize = mSize * 2.0;
    ExplosionBatch &batch = _batch();
    std::vector<ExplosionBatch::Span> &spans = batch.getSpans();
    const size_t crowd = batch.collectSpans(mLevel, mSource, (float) explosionSize, spans);
    mCrowd = crowd;
    mSpansSerial = mOwner.getActorSetSerial();
    size_t nearby = crowd;
    mHullKnown = false;
    // counted before any block is read: most explosions of a scattered tick have nobody in reach, and the
    // few targets of the others stand in a box far smaller than the whole reach
    if (crowd < COUNTED_CROWD) {
        nearby = 0;
        mHullLow = Vector3i((int32_t) std::floor(mSource.x), (int32_t) std::floor(mSource.y),
                            (int32_t) std::floor(mSource.z));
        mHullHigh = mHullLow;
        for (const ExplosionBatch::Span &span: spans) {
            for (size_t offset = 0; offset < span.mCount; ++offset) {
                const ExplosionBatch::Target &target = span.mFirst[offset];
                if (target.mActor == mSourceActor || !maybeWithin(target.mPosition, mSource, explosionSize)
                    || distanceBetween(target.mPosition, mSource) / explosionSize > 1.0)
                    continue;
                ++nearby;
                ActorSize size;
                size.mWidth = target.mWidth;
                size.mHeight = target.mHeight;
                const AxisAlignedBB box = boundingBoxOf(target.mPosition, size);
                mHullLow = Vector3i(std::min(mHullLow.x, (int32_t) std::floor(box.mMinX)),
                                    std::min(mHullLow.y, (int32_t) std::floor(box.mMinY)),
                                    std::min(mHullLow.z, (int32_t) std::floor(box.mMinZ)));
                mHullHigh = Vector3i(std::max(mHullHigh.x, (int32_t) std::floor(box.mMaxX)),
                                     std::max(mHullHigh.y, (int32_t) std::floor(box.mMaxY)),
                                     std::max(mHullHigh.z, (int32_t) std::floor(box.mMaxZ)));
            }
        }
        mHullKnown = true;
    }
    mNear = nearby;
    return nearby;
}

BlastField &Explosion::_field(FieldUse use, bool sums) {
    BlastField &field = _batch().getField();
    const Vector3i center((int32_t) std::floor(mSource.x), (int32_t) std::floor(mSource.y),
                          (int32_t) std::floor(mSource.z));
    const int32_t rays = BlastField::rayReach(mSize);
    const int32_t whole = BlastField::targetReach(mSize);
    Vector3i low(center.x - whole, center.y - whole, center.z - whole);
    Vector3i high(center.x + whole, center.y + whole, center.z + whole);
    // the box of the targets when it is known, the rays around the source when they are cast
    if (use != FieldUse::Anything && (_near() == 0 || mHullKnown)) {
        const bool withRays = use == FieldUse::Rays;
        const bool withTargets = _near() != 0;
        low = withTargets ? mHullLow : center;
        high = withTargets ? mHullHigh : center;
        if (withRays) {
            low = Vector3i(std::min(low.x, center.x - rays), std::min(low.y, center.y - rays),
                           std::min(low.z, center.z - rays));
            high = Vector3i(std::max(high.x, center.x + rays), std::max(high.y, center.y + rays),
                            std::max(high.z, center.z + rays));
        }
    }
    field.prepare(mLevel, mSource, mSize, low, high, use != FieldUse::Rays && sums, _batch().getUpcoming(),
                  _batch().getUpcomingCount());
    return field;
}

namespace {
    // every ray lies in the cone from the source to the target: cut in slices one block thick along the
    // axis the target is farthest on, each slice is a small box; all of them empty, nothing hides the target.
    // far tighter than the one box around both, which a slope between them fills with blocks they do not cross
    bool isConeClear(const BlastField &field, const Vector3f &source, const AxisAlignedBB &box) {
        const double from[3] = {source.x, source.y, source.z};
        const double low[3] = {box.mMinX, box.mMinY, box.mMinZ};
        const double high[3] = {box.mMaxX, box.mMaxY, box.mMaxZ};

        // the axis along which the target stands clear of the source by the most
        int axis = -1;
        double gap = 0.0;
        for (int index = 0; index < 3; ++index) {
            const double apart = std::max(low[index] - from[index], from[index] - high[index]);
            if (apart > gap) {
                gap = apart;
                axis = index;
            }
        }
        if (axis < 0)
            return false;

        const int side1 = (axis + 1) % 3;
        const int side2 = (axis + 2) % 3;
        const bool ahead = low[axis] > from[axis];
        // distances along the axis, from the source: the near and far faces of the target
        const double nearFace = ahead ? low[axis] - from[axis] : from[axis] - high[axis];
        const double farFace = ahead ? high[axis] - from[axis] : from[axis] - low[axis];
        const int32_t firstCell = (int32_t) std::floor(from[axis]);
        const int32_t lastCell = (int32_t) std::floor(ahead ? high[axis] : low[axis]);
        const int32_t step = ahead ? 1 : -1;

        for (int32_t cell = firstCell;; cell += step) {
            // the stretch of the axis this slice covers, as distances from the source
            const double enter = std::max(0.0, ahead ? (double) cell - from[axis] : from[axis] - (double) (cell + 1));
            const double leave = std::min(farFace, ahead ? (double) (cell + 1) - from[axis] : from[axis] - (double) cell);
            // how far along its ray a point of the slice can be: least on the rays to the far face, most on
            // the rays to the near one
            const double least = enter / farFace;
            const double most = std::min(1.0, leave / nearFace);

            int32_t corner[3][2];
            corner[axis][0] = cell;
            corner[axis][1] = cell;
            for (const int side: {side1, side2}) {
                const double toLow = low[side] - from[side];
                const double toHigh = high[side] - from[side];
                const double smallest = std::min(std::min(least * toLow, most * toLow),
                                                 std::min(least * toHigh, most * toHigh));
                const double largest = std::max(std::max(least * toLow, most * toLow),
                                                std::max(least * toHigh, most * toHigh));
                corner[side][0] = (int32_t) std::floor(from[side] + smallest - CONE_SLACK);
                corner[side][1] = (int32_t) std::floor(from[side] + largest + CONE_SLACK);
            }
            if (!field.isBoxClear(corner[0][0], corner[1][0], corner[2][0], corner[0][1], corner[1][1], corner[2][1]))
                return false;
            if (cell == lastCell)
                return true;
        }
    }
}

namespace {
    // 1 when nothing can hide the target, -1 when the rays have to be walked
    float quickDensityOf(const BlastField &field, const Vector3f &source, const AxisAlignedBB &boundingBox) {
        if (boundingBox.isVectorInside(source))
            return 1.0f;

        // every ray stays in the box around the source and the target: nothing in it, nothing hides the target
        const int32_t minX = (int32_t) std::floor(std::min(source.x, boundingBox.mMinX));
        const int32_t minY = (int32_t) std::floor(std::min(source.y, boundingBox.mMinY));
        const int32_t minZ = (int32_t) std::floor(std::min(source.z, boundingBox.mMinZ));
        const int32_t maxX = (int32_t) std::floor(std::max(source.x, boundingBox.mMaxX));
        const int32_t maxY = (int32_t) std::floor(std::max(source.y, boundingBox.mMaxY));
        const int32_t maxZ = (int32_t) std::floor(std::max(source.z, boundingBox.mMaxZ));
        if (field.isBoxClear(minX, minY, minZ, maxX, maxY, maxZ))
            return 1.0f;
        return isConeClear(field, source, boundingBox) ? 1.0f : -1.0f;
    }
}

float Explosion::_quickDensity(const AxisAlignedBB &boundingBox) const {
    return quickDensityOf(mBatch->getField(), mSource, boundingBox);
}

namespace {
    // the box around one ray: nothing in it, the ray is free without being walked
    bool isWayOpen(const BlastField &field, const Vector3f &source, double x, double y, double z) {
        return field.isBoxClear((int32_t) std::floor(std::min((double) source.x, x)),
                                (int32_t) std::floor(std::min((double) source.y, y)),
                                (int32_t) std::floor(std::min((double) source.z, z)),
                                (int32_t) std::floor(std::max((double) source.x, x)),
                                (int32_t) std::floor(std::max((double) source.y, y)),
                                (int32_t) std::floor(std::max((double) source.z, z)));
    }
}

namespace {
    // the box around the rays to one row of the target, from one side of it to the other
    bool isRowOpen(const BlastField &field, const Vector3f &source, double x, double y, double minZ, double maxZ) {
        return field.isBoxClear((int32_t) std::floor(std::min((double) source.x, x)),
                                (int32_t) std::floor(std::min((double) source.y, y)),
                                (int32_t) std::floor(std::min((double) source.z, minZ)),
                                (int32_t) std::floor(std::max((double) source.x, x)),
                                (int32_t) std::floor(std::max((double) source.y, y)),
                                (int32_t) std::floor(std::max((double) source.z, maxZ)));
    }
}

namespace {
    // from any thread: reads the field alone, -1 when a ray meets a block whose shape has to be asked
    float walkedDensityOf(const BlastField &field, const Vector3f &source, const AxisAlignedBB &boundingBox) {
        bool unsure = false;
        const float density = densityThrough(
                [&field, &unsure, source](double x, double y, double z) {
                    if (unsure || isWayOpen(field, source, x, y, z))
                        return false;
                    const int stopped = field.walk(source.x, source.y, source.z, x, y, z, 0.3);
                    if (stopped < 0)
                        unsure = true;
                    return stopped == 1;
                },
                [&field, source, &boundingBox](double x, double y) {
                    return isRowOpen(field, source, x, y, boundingBox.mMinZ, boundingBox.mMaxZ);
                },
                source, boundingBox);
        return unsure ? -1.0f : density;
    }
}

float Explosion::_walkedDensity(const AxisAlignedBB &boundingBox) const {
    return walkedDensityOf(mBatch->getField(), mSource, boundingBox);
}

float Explosion::_density(const AxisAlignedBB &boundingBox) {
    // the blocks read for the rays or for the actors most often hold this target too
    const Vector3i low((int32_t) std::floor(std::min(mSource.x, boundingBox.mMinX)),
                       (int32_t) std::floor(std::min(mSource.y, boundingBox.mMinY)),
                       (int32_t) std::floor(std::min(mSource.z, boundingBox.mMinZ)));
    const Vector3i high((int32_t) std::floor(std::max(mSource.x, boundingBox.mMaxX)),
                        (int32_t) std::floor(std::max(mSource.y, boundingBox.mMaxY)),
                        (int32_t) std::floor(std::max(mSource.z, boundingBox.mMaxZ)));
    // worked out apart: the blocks read for all of them hold the answer while they are kept true
    const BlastField *apart = mApart == Apart::Done ? _batch().getApartField() : nullptr;
    if (apart != nullptr && apart->contains(low, high)) {
        const float known = quickDensityOf(*apart, mSource, boundingBox);
        const float walked = known < 0.0f ? walkedDensityOf(*apart, mSource, boundingBox) : known;
        if (walked >= 0.0f)
            return walked;
    }
    BlastField &field = _batch().getField().holds(mLevel, low, high) ? _batch().getField()
                                                                      : _field(FieldUse::Anything);
    const float quick = _quickDensity(boundingBox);
    if (quick >= 0.0f)
        return quick;

    Level &level = mLevel;
    const Vector3f source = mSource;
    const auto blocked = [&field, &level](int32_t x, int32_t y, int32_t z, float pointX, float pointY,
                                          float pointZ) {
        const int32_t index = field.indexOf(x, y, z);
        if (index < 0) {
            const BlockState *state = level.peekBlockPtr(x, y, z);
            return state != nullptr && BlockShape::isPositionInside(*state, x, y, z, pointX, pointY, pointZ);
        }
        if (!field.isBlocking(index))
            return false;
        return field.isFullCube(index) || field.isPositionInside(index, x, y, z, pointX, pointY, pointZ);
    };
    return densityThrough(
            [&](double x, double y, double z) {
                return !isWayOpen(field, source, x, y, z)
                       && rayBlocked(blocked, source.x, source.y, source.z, x, y, z, 0.3);
            },
            [&field, source, &boundingBox](double x, double y) {
                return isRowOpen(field, source, x, y, boundingBox.mMinZ, boundingBox.mMaxZ);
            },
            source, boundingBox);
}

int BlastField::walk(double srcX, double srcY, double srcZ, double dstX, double dstY, double dstZ,
                     double stepSize) const {
    const double directionX = dstX - srcX;
    const double directionY = dstY - srcY;
    const double directionZ = dstZ - srcZ;
    if (directionX == 0.0 && directionY == 0.0 && directionZ == 0.0)
        return 0;
    if (!mCovers)
        return -1;

    const double length = std::sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
    const double normalX = directionX / length;
    const double normalY = directionY / length;
    const double normalZ = directionZ / length;
    const uint32_t dimX = (uint32_t) mDimX;
    const uint32_t dimY = (uint32_t) mDimY;
    const uint32_t dimZ = (uint32_t) mDimZ;
    const uint8_t *blocking = mBlocking.data();

    // four samples a turn: the reads do not wait on one another
    double t = 0.0;
    for (;;) {
        uint8_t kinds[4];
        int taken = 0;
        for (; taken < 4 && t < length; ++taken, t += stepSize) {
            const uint32_t x = (uint32_t) ((int32_t) std::floor(srcX + normalX * t) - mOriginX);
            const uint32_t y = (uint32_t) ((int32_t) std::floor(srcY + normalY * t) - mOriginY);
            const uint32_t z = (uint32_t) ((int32_t) std::floor(srcZ + normalZ * t) - mOriginZ);
            if (x >= dimX || y >= dimY || z >= dimZ) {
                kinds[taken] = OUTSIDE;
                continue;
            }
            kinds[taken] = blocking[(x * dimZ + z) * dimY + y];
        }
        for (int index = 0; index < taken; ++index) {
            if (kinds[index] != 0)
                return kinds[index] == FULL_CUBE ? 1 : -1;
        }
        if (taken < 4)
            return 0;
    }
}

bool BlastField::_stillHolds(Level &level, const Vector3i &low, const Vector3i &high) const {
    if (!mRead || !mCovers || mLevel != &level)
        return false;

    if (low.x < mOriginX || low.y < mOriginY || low.z < mOriginZ || high.x >= mOriginX + mDimX
        || high.y >= mOriginY + mDimY || high.z >= mOriginZ + mDimZ) {
        return false;
    }

    for (const ChunkMark &mark: mChunks) {
        const LevelChunk *chunk = level.peekChunkPtr(mark.mX, mark.mZ);
        if (chunk != mark.mChunk || (chunk != nullptr && chunk->getNetworkRevision() != mark.mRevision)) {
            return false;
        }
    }
    return true;
}

int32_t BlastField::rayReach(double size) {
    // a ray dies after 1.3 * size / 0.225 steps of 0.3
    return (int32_t) std::ceil(size * 1.3 / 0.225 * 0.3);
}

int32_t BlastField::targetReach(double size) {
    // a target stands up to twice the size away, its box a little further
    return (int32_t) std::ceil(size * 2.0) + FIELD_MARGIN;
}

void BlastField::prepare(Level &level, const Vector3f &source, double size, const Vector3i &needLow,
                         const Vector3i &needHigh, bool sums, const Vector3f *upcoming, size_t upcomingCount) {
    mFailed = false;
    const int32_t radius = targetReach(size);
    if (radius > FIELD_MAX_RADIUS) {
        mRead = true;
        mCovers = false;
        return;
    }

    const Vector3i center((int32_t) std::floor(source.x), (int32_t) std::floor(source.y),
                          (int32_t) std::floor(source.z));
    if (!_stillHolds(level, needLow, needHigh)) {
        Vector3i low = needLow;
        Vector3i high = needHigh;

        // nothing changed under the last read: the tick has many explosions over untouched blocks (water,
        // air), and one read around the close ones to come serves them all
        const size_t lookAhead = std::min(upcomingCount, FIELD_WIDE_LOOKAHEAD);
        bool quiet = lookAhead >= FIELD_WIDE_WORTH && mRead && mCovers && mLevel == &level
                     && mWideWasted < FIELD_WIDE_WASTES;
        for (size_t index = 0; quiet && index < mChunks.size(); ++index) {
            const LevelChunk *chunk = level.peekChunkPtr(mChunks[index].mX, mChunks[index].mZ);
            quiet = chunk == mChunks[index].mChunk
                    && (chunk == nullptr || chunk->getNetworkRevision() == mChunks[index].mRevision);
        }
        // a wide read thrown away by a broken block before it served its share: after a few of them the
        // explosions of the tick are breaking blocks all over, and each keeps to its own small read
        if (mWide && !quiet && mWideServed < FIELD_WIDE_WORTH / 2)
            ++mWideWasted;

        // the widest box around the close ones that still costs less than their own small reads
        // the explosions come sorted by place: the close ones are among the next few hundred
        bool wide = false;
        for (int32_t reach = FIELD_WIDE_REACH; quiet && !wide && reach >= FIELD_WIDE_REACH / 2; reach /= 2) {
            size_t close = 0;
            Vector3i closeLow = low;
            Vector3i closeHigh = high;
            for (size_t index = 0; index < lookAhead; ++index) {
                const Vector3i other((int32_t) std::floor(upcoming[index].x), (int32_t) std::floor(upcoming[index].y),
                                     (int32_t) std::floor(upcoming[index].z));
                if (std::abs(other.x - center.x) > reach || std::abs(other.y - center.y) > reach
                    || std::abs(other.z - center.z) > reach)
                    continue;
                ++close;
                closeLow = Vector3i(std::min(closeLow.x, other.x - radius), std::min(closeLow.y, other.y - radius),
                                    std::min(closeLow.z, other.z - radius));
                closeHigh = Vector3i(std::max(closeHigh.x, other.x + radius), std::max(closeHigh.y, other.y + radius),
                                     std::max(closeHigh.z, other.z + radius));
            }
            const size_t volume = (size_t) (closeHigh.x - closeLow.x + 1) * (size_t) (closeHigh.y - closeLow.y + 1)
                                  * (size_t) (closeHigh.z - closeLow.z + 1);
            if (close >= FIELD_WIDE_WORTH && volume <= close * FIELD_WIDE_CELLS && volume <= FIELD_WIDE_LIMIT) {
                wide = true;
                low = closeLow;
                high = closeHigh;
            }
        }
        _read(level, low, high);
        mWide = wide;
        mWideServed = 0;
    }
    ++mWideServed;
    if (sums && !mSummed) {
        mBlockingPrefix.clear();
        if (mAnyBlocking)
            _sum(mBlocking, mBlockingPrefix);
        mSummed = true;
    }

    const int32_t rays = rayReach(size);
    mReachMinX = center.x - rays - mOriginX;
    mReachMinY = center.y - rays - mOriginY;
    mReachMinZ = center.z - rays - mOriginZ;
    mReachMaxX = mReachMinX + rays * 2 + 1;
    mReachMaxY = mReachMinY + rays * 2 + 1;
    mReachMaxZ = mReachMinZ + rays * 2 + 1;
}

bool BlastField::findBlocksInReach(Vector3i &low, Vector3i &high) const {
    if (!mCovers || mReachMinX < 0 || mReachMinY < 0 || mReachMinZ < 0 || mReachMaxX > mDimX || mReachMaxY > mDimY
        || mReachMaxZ > mDimZ) {
        low = Vector3i(INT32_MIN / 2, INT32_MIN / 2, INT32_MIN / 2);
        high = Vector3i(INT32_MAX / 2, INT32_MAX / 2, INT32_MAX / 2);
        return true;
    }

    // a read no wider than the reach already knows the box of its blocks
    if ((size_t) mDimX * (size_t) mDimY * (size_t) mDimZ
        <= (size_t) (mReachMaxX - mReachMinX) * (size_t) (mReachMaxY - mReachMinY)
                   * (size_t) (mReachMaxZ - mReachMinZ) * 2) {
        low = mBlocksLow;
        high = mBlocksHigh;
        return mAnyBlock;
    }

    bool any = false;
    low = Vector3i(INT32_MAX, INT32_MAX, INT32_MAX);
    high = Vector3i(INT32_MIN, INT32_MIN, INT32_MIN);
    for (int32_t x = mReachMinX; x < mReachMaxX; ++x) {
        for (int32_t z = mReachMinZ; z < mReachMaxZ; ++z) {
            const BlockState *const *column = mStates.data() + (size_t) ((x * mDimZ + z) * mDimY);
            int32_t first = -1;
            int32_t last = -1;
            for (int32_t y = mReachMinY; y < mReachMaxY; ++y) {
                if (column[y] == nullptr)
                    continue;
                if (first < 0)
                    first = y;
                last = y;
            }
            if (first < 0)
                continue;

            any = true;
            low = Vector3i(std::min(low.x, x + mOriginX), std::min(low.y, first + mOriginY),
                           std::min(low.z, z + mOriginZ));
            high = Vector3i(std::max(high.x, x + mOriginX), std::max(high.y, last + mOriginY),
                            std::max(high.z, z + mOriginZ));
        }
    }
    return any;
}

void BlastField::_read(Level &level, const Vector3i &low, const Vector3i &high) {
    mRead = true;
    mCovers = true;
    mLevel = &level;
    ++mStamp;

    mDimX = high.x - low.x + 1;
    mDimY = high.y - low.y + 1;
    mDimZ = high.z - low.z + 1;
    mOriginX = low.x;
    mOriginY = low.y;
    mOriginZ = low.z;

    const size_t cells = (size_t) mDimX * (size_t) mDimY * (size_t) mDimZ;
    mStates.assign(cells, nullptr);
    mBlocking.assign(cells, 0);
    if (mKeepShapes && mShapeStamps.size() < cells) {
        mShapeStamps.assign(cells, 0);
        mShapes.resize(cells);
    }
    if (mResistances.size() < cells)
        mResistances.resize(cells);

    mAnyBlock = false;
    mBlocksLow = Vector3i(INT32_MAX, INT32_MAX, INT32_MAX);
    mBlocksHigh = Vector3i(INT32_MIN, INT32_MIN, INT32_MIN);

    mChunks.clear();
    for (int32_t chunkX = mOriginX >> 4; chunkX <= (mOriginX + mDimX - 1) >> 4; ++chunkX) {
        for (int32_t chunkZ = mOriginZ >> 4; chunkZ <= (mOriginZ + mDimZ - 1) >> 4; ++chunkZ) {
            const LevelChunk *chunk = level.peekChunkPtr(chunkX, chunkZ);
            mChunks.push_back(ChunkMark{chunkX, chunkZ, chunk, chunk == nullptr ? 0 : chunk->getNetworkRevision()});
        }
    }

    const BlockState *air = airState();
    const BlockState *lastState = nullptr;
    uint8_t lastBlocking = 0;
    float lastResistance = 0.0f;
    bool lastAir = false;
    const int32_t bottom = std::max(mOriginY, LevelChunk::MIN_Y);
    const int32_t top = std::min(mOriginY + mDimY - 1, LevelChunk::MAX_Y);
    LevelChunk *chunk = nullptr;
    int32_t chunkX = 0;
    int32_t chunkZ = 0;
    bool haveChunk = false;
    bool anyBlocking = false;

    for (int32_t localX = 0; localX < mDimX; ++localX) {
        const int32_t x = mOriginX + localX;
        for (int32_t localZ = 0; localZ < mDimZ; ++localZ) {
            const int32_t z = mOriginZ + localZ;
            if (!haveChunk || chunkX != (x >> 4) || chunkZ != (z >> 4)) {
                chunkX = x >> 4;
                chunkZ = z >> 4;
                chunk = level.peekChunkPtr(chunkX, chunkZ);
                haveChunk = true;
                // unpacking is the tick thread's to do
                if (chunk != nullptr && mHelperOwned && chunk->isPacked()) {
                    mFailed = true;
                    mRead = false;
                    mCovers = false;
                    return;
                }
            }
            if (chunk == nullptr)
                continue;

            const size_t column = (size_t) ((localX * mDimZ + localZ) * mDimY);
            int32_t columnLow = INT32_MAX;
            int32_t columnHigh = INT32_MIN;
            for (int32_t y = bottom; y <= top;) {
                const int32_t sectionEnd = std::min(top, y | 15);
                const SubChunk &section = chunk->getSubChunk(LevelChunk::subChunkIndexFor(y));
                if (section.isEmpty()) {
                    y = sectionEnd + 1;
                    continue;
                }
                const bool layerOneEmpty = section.isLayerEmpty(1);
                for (; y <= sectionEnd; ++y) {
                    const BlockState *state = &section.getBlock(x & 15, y & 15, z & 15);
                    if (state == air)
                        continue;

                    // blocks come in runs of one state: its name and its kind are asked once per run
                    if (state != lastState) {
                        lastState = state;
                        lastAir = state->mName == BlockNames::AIR;
                        const auto known = mBlockingStates.find(state);
                        if (known != mBlockingStates.end()) {
                            lastBlocking = known->second;
                        } else {
                            // one thread at a time asks the blocks for a shape
                            static std::mutex shapeMutex;
                            std::lock_guard<std::mutex> shapeLock(shapeMutex);
                            const AxisAlignedBB shape = BlockShape::getShapeAt(*state, 0, 0, 0);
                            const bool whole = BlockShape::isFullCube(*state) && shape.mMinX <= 0.0f
                                               && shape.mMinY <= 0.0f && shape.mMinZ <= 0.0f && shape.mMaxX >= 1.0f
                                               && shape.mMaxY >= 1.0f && shape.mMaxZ >= 1.0f;
                            lastBlocking = whole ? FULL_CUBE
                                                 : BlockShape::hasCollision(*state) || !shape.isEmpty() ? PARTIAL : 0;
                            mBlockingStates.emplace(state, lastBlocking);
                        }
                        const auto resists = mResistStates.find(state);
                        lastResistance = resists != mResistStates.end()
                                                 ? resists->second
                                                 : mResistStates.emplace(state, resistanceOf(*state)).first->second;
                    }
                    if (lastAir)
                        continue;

                    const size_t cell = column + (size_t) (y - mOriginY);
                    mStates[cell] = state;
                    // the stronger layer resists: waterlogged water shields like water
                    float resistance = lastResistance;
                    if (resistance != -1.0f && !layerOneEmpty)
                        resistance = std::max(resistance, resistanceOf(section.getBlock(x & 15, y & 15, z & 15, 1)));
                    mResistances[cell] = resistance;
                    if (y < columnLow)
                        columnLow = y;
                    columnHigh = y;
                    if (lastBlocking != 0) {
                        mBlocking[cell] = lastBlocking;
                        anyBlocking = true;
                    }
                }
            }
            if (columnLow <= columnHigh)
                _noteColumn(x, z, columnLow, columnHigh);
        }
    }

    mAnyBlocking = anyBlocking;
    mSummed = false;
}

void BlastField::readForMany(Level &level, const Vector3i &low, const Vector3i &high) {
    mKeepShapes = false;
    mHelperOwned = false;
    mFailed = false;
    _read(level, low, high);
    resum();
    if (mRayStamps.size() < mStates.size())
        mRayStamps.assign(mStates.size(), 0);
}

void BlastField::resum() {
    mBlockingPrefix.clear();
    if (mAnyBlocking)
        _sum(mBlocking, mBlockingPrefix);
    mSummed = true;
}

bool BlastField::patch(Level &level, int32_t x, int32_t y, int32_t z) {
    const int32_t index = indexOf(x, y, z);
    if (index < 0)
        return false;

    const size_t cell = (size_t) index;
    const BlockState *state = level.peekBlockPtr(x, y, z);
    if (state == nullptr || state->mName == BlockNames::AIR) {
        // gone: the sums still count it, which only makes them say less often that a way is free
        mStates[cell] = nullptr;
        mBlocking[cell] = 0;
        return false;
    }

    auto known = mBlockingStates.find(state);
    if (known == mBlockingStates.end()) {
        const AxisAlignedBB shape = BlockShape::getShapeAt(*state, 0, 0, 0);
        const bool whole = BlockShape::isFullCube(*state) && shape.mMinX <= 0.0f && shape.mMinY <= 0.0f
                           && shape.mMinZ <= 0.0f && shape.mMaxX >= 1.0f && shape.mMaxY >= 1.0f && shape.mMaxZ >= 1.0f;
        known = mBlockingStates
                        .emplace(state, whole ? FULL_CUBE
                                              : BlockShape::hasCollision(*state) || !shape.isEmpty() ? PARTIAL : 0)
                        .first;
    }
    float resistance = resistanceOf(*state);
    if (resistance != -1.0f) {
        const BlockState *layerOne = level.peekBlockPtr(x, y, z, 1);
        if (layerOne != nullptr)
            resistance = std::max(resistance, resistanceOf(*layerOne));
    }

    const bool appeared = known->second != 0 && mBlocking[cell] == 0;
    mStates[cell] = state;
    mResistances[cell] = resistance;
    mBlocking[cell] = known->second;
    if (appeared)
        mAnyBlocking = true;
    return appeared;
}

bool BlastField::scanBlocks(const Vector3i &center, int32_t reach, Vector3i &low, Vector3i &high, bool &any) const {
    const int32_t minX = center.x - reach - mOriginX;
    const int32_t minY = center.y - reach - mOriginY;
    const int32_t minZ = center.z - reach - mOriginZ;
    const int32_t side = reach * 2 + 1;
    if (!mCovers || minX < 0 || minY < 0 || minZ < 0 || minX + side > mDimX || minY + side > mDimY
        || minZ + side > mDimZ)
        return false;

    any = false;
    low = Vector3i(INT32_MAX, INT32_MAX, INT32_MAX);
    high = Vector3i(INT32_MIN, INT32_MIN, INT32_MIN);
    for (int32_t x = minX; x < minX + side; ++x) {
        for (int32_t z = minZ; z < minZ + side; ++z) {
            const BlockState *const *column = mStates.data() + (size_t) ((x * mDimZ + z) * mDimY);
            int32_t first = -1;
            int32_t last = -1;
            for (int32_t y = minY; y < minY + side; ++y) {
                if (column[y] == nullptr)
                    continue;
                if (first < 0)
                    first = y;
                last = y;
            }
            if (first < 0)
                continue;

            any = true;
            low = Vector3i(std::min(low.x, x + mOriginX), std::min(low.y, first + mOriginY), std::min(low.z, z + mOriginZ));
            high = Vector3i(std::max(high.x, x + mOriginX), std::max(high.y, last + mOriginY),
                            std::max(high.z, z + mOriginZ));
        }
    }
    return true;
}

void BlastField::_noteColumn(int32_t x, int32_t z, int32_t low, int32_t high) {
    mAnyBlock = true;
    mBlocksLow = Vector3i(std::min(mBlocksLow.x, x), std::min(mBlocksLow.y, low), std::min(mBlocksLow.z, z));
    mBlocksHigh = Vector3i(std::max(mBlocksHigh.x, x), std::max(mBlocksHigh.y, high), std::max(mBlocksHigh.z, z));
}

void BlastField::_sum(const std::vector<uint8_t> &cells, std::vector<int32_t> &prefix) const {
    prefix.assign((size_t) (mDimX + 1) * (size_t) (mDimY + 1) * (size_t) (mDimZ + 1), 0);
    for (int32_t x = 0; x < mDimX; ++x) {
        for (int32_t z = 0; z < mDimZ; ++z) {
            int32_t run = 0;
            const size_t column = (size_t) ((x * mDimZ + z) * mDimY);
            for (int32_t y = 0; y < mDimY; ++y) {
                run += cells[column + (size_t) y] != 0 ? 1 : 0;
                prefix[_prefix(x + 1, y + 1, z + 1)] = run + prefix[_prefix(x, y + 1, z + 1)]
                                                       + prefix[_prefix(x + 1, y + 1, z)]
                                                       - prefix[_prefix(x, y + 1, z)];
            }
        }
    }
}

bool BlastField::isPositionInside(int32_t index, int32_t x, int32_t y, int32_t z, float pointX, float pointY,
                                  float pointZ) {
    // the shape of a block is asked once per explosion
    if (mShapeStamps[(size_t) index] != mStamp) {
        mShapeStamps[(size_t) index] = mStamp;
        mShapes[(size_t) index] = BlockShape::getShapeAt(*mStates[(size_t) index], x, y, z);
    }

    const AxisAlignedBB &shape = mShapes[(size_t) index];
    return !shape.isEmpty() && shape.isVectorInside(pointX, pointY, pointZ);
}

int64_t ExplosionBatch::_cellOf(const Vector3f &position) {
    return _cell((int32_t) std::floor(position.x) >> CELL_SHIFT, (int32_t) std::floor(position.z) >> CELL_SHIFT);
}

void ExplosionBatch::_addTarget(ServerActor &actor) {
    const ActorSize size = actor.getSize();
    Target target;
    target.mActor = &actor;
    target.mPosition = actor.getPosition();
    target.mWidth = size.mWidth;
    target.mHeight = size.mHeight;
    target.mInvulnerable = actor.isInvulnerable();
    target.mIndex = mTargetCount++;
    mTargets[_cellOf(target.mPosition)].push_back(target);
}

void ExplosionBatch::_applyChanges(Level &level) {
    const std::vector<ServerNetworkHandler::ActorChange> &changes = *mOwner.getActorChanges();
    const auto takeOut = [](std::vector<Target> &targets, const ServerActor *actor) {
        for (size_t index = 0; index < targets.size(); ++index) {
            if (targets[index].mActor != actor)
                continue;
            targets[index] = targets.back();
            targets.pop_back();
            return true;
        }
        return false;
    };

    for (; mJoinsRead < changes.size(); ++mJoinsRead) {
        const ServerNetworkHandler::ActorChange &change = changes[mJoinsRead];
        if (change.mJoined) {
            // gone again since: only its address is left, not to be read
            bool left = false;
            for (size_t later = mJoinsRead + 1; later < changes.size() && !left; ++later)
                left = !changes[later].mJoined && changes[later].mActor == change.mActor;
            if (left) {
                mSkippedLeavers.push_back(change.mActor);
                continue;
            }
            if (change.mActor->isIn(level) && change.mActor->isAlive())
                _addTarget(*change.mActor);
            continue;
        }

        const auto skipped = std::find(mSkippedLeavers.begin(), mSkippedLeavers.end(), change.mActor);
        if (skipped != mSkippedLeavers.end()) {
            mSkippedLeavers.erase(skipped);
            continue;
        }

        // listed where it stood when the buckets were made, most often where it still is; or not listed
        // at all (dead, elsewhere)
        const auto home = mTargets.find(_cellOf(change.mPosition));
        if (home != mTargets.end() && takeOut(home->second, change.mActor))
            continue;
        for (auto &cell: mTargets) {
            if (takeOut(cell.second, change.mActor))
                break;
        }
    }
}

void ExplosionBatch::_syncTargets(Level &level) {
    if (mTargetLevel == &level && mTargetSerial == mOwner.getActorSetSerial())
        return;

    // only a few came or went since the buckets were made: they are added or taken out where they stand
    const std::vector<ServerNetworkHandler::ActorChange> *changes =
            mFollowingJoins ? mOwner.getActorChanges() : nullptr;
    if (mTargetLevel == &level && changes != nullptr && mJoinsRead <= changes->size()) {
        _applyChanges(level);
        mTargetSerial = mOwner.getActorSetSerial();
        return;
    }

    mTargetLevel = &level;
    mTargetSerial = mOwner.getActorSetSerial();
    mJoinsRead = changes == nullptr ? 0 : changes->size();
    for (auto &cell: mTargets)
        cell.second.clear();
    mTargetCount = 0;

    for (auto &entry: mOwner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor != nullptr && actor->isIn(level) && actor->isAlive())
            _addTarget(*actor);
    }
}

namespace {
    // every core for a moment, on work cut in pieces that do not wait on one another
    class QuietWorkers {
    public:
        static QuietWorkers &get() {
            static QuietWorkers workers;
            return workers;
        }

        size_t size() const {
            return mThreads.size() + 1;
        }

        // job(worker) on every thread, the caller as worker 0; back once all returned
        void run(const std::function<void(size_t)> &job) {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mJob = &job;
                mLeft = mThreads.size();
                ++mRound;
            }
            mWake.notify_all();
            job(0);
            std::unique_lock<std::mutex> lock(mMutex);
            mDone.wait(lock, [this] { return mLeft == 0; });
            mJob = nullptr;
        }

        ~QuietWorkers() {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStop = true;
            }
            mWake.notify_all();
            for (std::thread &thread: mThreads)
                thread.join();
        }

    private:
        QuietWorkers() {
            const unsigned cores = std::thread::hardware_concurrency();
            const unsigned helpers = cores > 1 ? std::min(cores - 1, 15u) : 0;
            for (unsigned index = 0; index < helpers; ++index)
                mThreads.emplace_back([this, index] {
#ifdef __APPLE__
                    // short bursts the tick waits on: they rank above background work, as the tick thread does
                    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
                    _loop(index + 1);
                });
        }

        void _loop(size_t worker) {
            uint64_t seen = 0;
            for (;;) {
                const std::function<void(size_t)> *job = nullptr;
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mWake.wait(lock, [&] { return mStop || mRound != seen; });
                    if (mStop)
                        return;
                    seen = mRound;
                    job = mJob;
                }
                (*job)(worker);
                std::lock_guard<std::mutex> lock(mMutex);
                if (--mLeft == 0)
                    mDone.notify_all();
            }
        }

        std::vector<std::thread> mThreads;
        std::mutex mMutex;
        std::condition_variable mWake;
        std::condition_variable mDone;
        const std::function<void(size_t)> *mJob = nullptr;
        size_t mLeft = 0;
        uint64_t mRound = 0;
        bool mStop = false;
    };

    // what one thread keeps while it settles quiet explosions
    struct QuietHand {
        BlastField mField;
        // per target, the pushes of the explosions this thread settled
        std::vector<Vector3f> mPushes;
        struct Seen {
            const ExplosionBatch::Target *mTarget;
            double mDistance;
        };
        std::vector<Seen> mSeen;
        std::vector<std::pair<size_t, ExplosionBatch::Hit>> mHits;
        std::vector<size_t> mLeftWhole;
    };
}

void ExplosionBatch::settleQuiet(Level &level, const std::vector<QuietBlast> &blasts) {
    mQuietSettled.assign(blasts.size(), 0);
    mQuietHits.assign(blasts.size(), {});
    if (blasts.empty())
        return;

    _syncTargets(level);
    std::vector<Vector3f> centers;
    centers.reserve(blasts.size());
    for (const QuietBlast &blast: blasts)
        centers.push_back(blast.mSource);

    const bool crowd = blasts.size() >= QUIET_CROWD;
    std::vector<QuietHand> hands(crowd ? QuietWorkers::get().size() : 1);
    std::atomic<size_t> next{0};
    const std::unordered_map<int64_t, std::vector<Target>> &buckets = mTargets;
    const uint32_t targetCount = mTargetCount;

    const auto settle = [&](size_t worker) {
        QuietHand &hand = hands[worker];
        hand.mField.setHelperOwned(crowd);
        hand.mPushes.assign(targetCount, Vector3f());
        for (;;) {
            const size_t first = next.fetch_add(QUIET_SHARE, std::memory_order_relaxed);
            if (first >= blasts.size())
                return;
            const size_t last = std::min(blasts.size(), first + QUIET_SHARE);
            for (size_t index = first; index < last; ++index) {
                const QuietBlast &blast = blasts[index];
                const Vector3f &source = blast.mSource;
                const double reach = blast.mSize * 2.0;

                // who stands in reach, and the box that holds them and the source
                hand.mSeen.clear();
                Vector3i low((int32_t) std::floor(source.x), (int32_t) std::floor(source.y),
                             (int32_t) std::floor(source.z));
                Vector3i high = low;
                const int32_t minX = (int32_t) std::floor(source.x - (float) reach) >> CELL_SHIFT;
                const int32_t maxX = (int32_t) std::floor(source.x + (float) reach) >> CELL_SHIFT;
                const int32_t minZ = (int32_t) std::floor(source.z - (float) reach) >> CELL_SHIFT;
                const int32_t maxZ = (int32_t) std::floor(source.z + (float) reach) >> CELL_SHIFT;
                for (int32_t cellX = minX; cellX <= maxX; ++cellX) {
                    for (int32_t cellZ = minZ; cellZ <= maxZ; ++cellZ) {
                        const auto found = buckets.find(_cell(cellX, cellZ));
                        if (found == buckets.end())
                            continue;
                        for (const Target &target: found->second) {
                            if (target.mActor == blast.mSourceActor || !maybeWithin(target.mPosition, source, reach))
                                continue;
                            const double distance = distanceBetween(target.mPosition, source) / reach;
                            if (distance > 1.0)
                                continue;
                            // gone at the end of the tick, and nothing hurts it: the push would move nothing
                            if (target.mInvulnerable && target.mActor->isExpired())
                                continue;

                            hand.mSeen.push_back(QuietHand::Seen{&target, distance});
                            ActorSize size;
                            size.mWidth = target.mWidth;
                            size.mHeight = target.mHeight;
                            const AxisAlignedBB box = boundingBoxOf(target.mPosition, size);
                            low = Vector3i(std::min(low.x, (int32_t) std::floor(box.mMinX)),
                                           std::min(low.y, (int32_t) std::floor(box.mMinY)),
                                           std::min(low.z, (int32_t) std::floor(box.mMinZ)));
                            high = Vector3i(std::max(high.x, (int32_t) std::floor(box.mMaxX)),
                                            std::max(high.y, (int32_t) std::floor(box.mMaxY)),
                                            std::max(high.z, (int32_t) std::floor(box.mMaxZ)));
                        }
                    }
                }
                if (hand.mSeen.empty()) {
                    mQuietSettled[index] = 1;
                    continue;
                }

                hand.mField.prepare(level, source, blast.mSize, low, high, hand.mSeen.size() > FEW_TARGETS,
                                    centers.data() + index, centers.size() - index);
                if (hand.mField.hasFailed()) {
                    hand.mLeftWhole.push_back(index);
                    continue;
                }

                for (const QuietHand::Seen &seen: hand.mSeen) {
                    const Target &target = *seen.mTarget;
                    ActorSize size;
                    size.mWidth = target.mWidth;
                    size.mHeight = target.mHeight;
                    const AxisAlignedBB box = boundingBoxOf(target.mPosition, size);
                    float density = quickDensityOf(hand.mField, source, box);
                    if (density < 0.0f)
                        density = walkedDensityOf(hand.mField, source, box);

                    // an invulnerable target takes the push alone: nothing else of the world is touched
                    if (density >= 0.0f && target.mInvulnerable) {
                        const Vector3f &position = target.mPosition;
                        const Vector3f motion = normalized(
                                Vector3f(position.x - source.x, position.y - source.y, position.z - source.z));
                        const double impact = (1.0 - seen.mDistance) * density;
                        Vector3f &push = hand.mPushes[target.mIndex];
                        push = Vector3f(push.x + motion.x * (float) impact, push.y + motion.y * (float) impact,
                                        push.z + motion.z * (float) impact);
                        continue;
                    }
                    hand.mHits.emplace_back(index, Hit{&target, seen.mDistance, density});
                }
                mQuietSettled[index] = 1;
            }
        }
    };
    if (crowd)
        QuietWorkers::get().run(settle);
    else
        settle(0);

    // the pushes of every thread, added up on each actor
    std::vector<ServerActor *> actors(targetCount, nullptr);
    for (const auto &bucket: mTargets) {
        for (const Target &target: bucket.second)
            actors[target.mIndex] = target.mActor;
    }
    for (uint32_t index = 0; index < targetCount; ++index) {
        Vector3f sum;
        for (const QuietHand &hand: hands) {
            if (hand.mPushes.empty())
                continue;
            const Vector3f &push = hand.mPushes[index];
            sum = Vector3f(sum.x + push.x, sum.y + push.y, sum.z + push.z);
        }
        if (actors[index] == nullptr || (sum.x == 0.0f && sum.y == 0.0f && sum.z == 0.0f))
            continue;
        const Vector3f current = actors[index]->getMotion();
        actors[index]->setMotion(Vector3f(current.x + sum.x, current.y + sum.y, current.z + sum.z));
    }

    for (QuietHand &hand: hands) {
        for (auto &hit: hand.mHits)
            mQuietHits[hit.first].push_back(hit.second);
        // an explosion a thread could not read the blocks for is done whole by the caller
        for (const size_t index: hand.mLeftWhole)
            mQuietSettled[index] = 0;
    }
}

size_t ExplosionBatch::collectSpans(Level &level, const Vector3f &center, float reach, std::vector<Span> &spans) {
    _syncTargets(level);
    spans.clear();
    size_t total = 0;
    const int32_t minX = (int32_t) std::floor(center.x - reach) >> CELL_SHIFT;
    const int32_t maxX = (int32_t) std::floor(center.x + reach) >> CELL_SHIFT;
    const int32_t minZ = (int32_t) std::floor(center.z - reach) >> CELL_SHIFT;
    const int32_t maxZ = (int32_t) std::floor(center.z + reach) >> CELL_SHIFT;
    for (int32_t cellX = minX; cellX <= maxX; ++cellX) {
        for (int32_t cellZ = minZ; cellZ <= maxZ; ++cellZ) {
            const auto found = mTargets.find(_cell(cellX, cellZ));
            if (found == mTargets.end())
                continue;

            const std::vector<Target> &targets = found->second;
            total += targets.size();
            for (size_t first = 0; first < targets.size(); first += SPAN)
                spans.push_back(Span{targets.data() + first, std::min(SPAN, targets.size() - first)});
        }
    }
    return total;
}

void ExplosionBatch::_syncItems(Level &level) {
    const std::vector<std::unique_ptr<ItemActor>> &items = mOwner.getItemEntities();
    if (mItemLevel != &level || items.size() < mItemsIndexed) {
        mItemLevel = &level;
        mItemsIndexed = 0;
        for (auto &cell: mItems)
            cell.second.clear();
    }

    // blocks broken by an earlier explosion of the tick drop items the next ones reach
    for (; mItemsIndexed < items.size(); ++mItemsIndexed) {
        ItemActor *item = items[mItemsIndexed].get();
        if (item != nullptr && item->isIn(level))
            mItems[_cellOf(item->getPosition())].push_back(item);
    }
}

void ExplosionBatch::runApart(Level &level, const std::vector<Explosion *> &explosions) {
    if (explosions.empty())
        return;

    _syncTargets(level);

    // the blocks around all of them, as far as that stays a reasonable read; an explosion past it is done
    // whole by the tick thread, in its turn
    Vector3i low(INT32_MAX, INT32_MAX, INT32_MAX);
    Vector3i high(INT32_MIN, INT32_MIN, INT32_MIN);
    std::vector<Vector3i> cells;
    cells.reserve(explosions.size());
    for (const Explosion *explosion: explosions) {
        const Vector3i cell((int32_t) std::floor(explosion->mSource.x), (int32_t) std::floor(explosion->mSource.y),
                            (int32_t) std::floor(explosion->mSource.z));
        cells.push_back(cell);
    }
    std::vector<int32_t> axis[3];
    for (const Vector3i &cell: cells) {
        axis[0].push_back(cell.x);
        axis[1].push_back(cell.y);
        axis[2].push_back(cell.z);
    }
    Vector3i middle;
    for (int index = 0; index < 3; ++index)
        std::nth_element(axis[index].begin(), axis[index].begin() + (std::ptrdiff_t) (axis[index].size() / 2),
                         axis[index].end());
    middle = Vector3i(axis[0][axis[0].size() / 2], axis[1][axis[1].size() / 2], axis[2][axis[2].size() / 2]);
    const int32_t margin = BlastField::targetReach(explosions.front()->mSize);
    for (const Vector3i &cell: cells) {
        if (std::abs(cell.x - middle.x) > APART_SPAN || std::abs(cell.z - middle.z) > APART_SPAN
            || std::abs(cell.y - middle.y) > APART_SPAN / 2)
            continue;
        low = Vector3i(std::min(low.x, cell.x - margin), std::min(low.y, cell.y - margin), std::min(low.z, cell.z - margin));
        high = Vector3i(std::max(high.x, cell.x + margin), std::max(high.y, cell.y + margin),
                        std::max(high.z, cell.z + margin));
    }
    mApartField.readForMany(level, low, high);
    const size_t cellCount = (size_t) (high.x - low.x + 1) * (size_t) (high.y - low.y + 1) * (size_t) (high.z - low.z + 1);

    // kept true as the explosions break blocks; a block appearing makes the sums wrong, mended at a pause
    mApartLive = true;
    mApartStale = false;
    level.setBlockWatcher([this, &level](int32_t x, int32_t y, int32_t z) {
        if (mApartField.patch(level, x, y, z))
            mApartStale = true;
    });
    struct WatcherScope {
        Level &mLevel;
        bool &mLive;
        ~WatcherScope() {
            mLevel.setBlockWatcher(nullptr);
            mLive = false;
        }
    } watcherScope{level, mApartLive};

    // rounds: a few explosions standing far enough from one another are worked out together, each one
    // reading blocks and pushing actors no other of the round reaches, then ended one by one here, where
    // everything that changes the world happens
    BlastWorkers::get().heat(true);
    struct HeatScope {
        ~HeatScope() {
            BlastWorkers::get().heat(false);
        }
    } heatScope;

    std::vector<uint8_t> ended(explosions.size(), 0);
    std::vector<size_t> round;
    size_t sharesSinceSum = 0;
    const size_t sumEvery = std::max<size_t>(APART_SUM_SHARES, cellCount / APART_SUM_CELLS);
    std::vector<Explosion *> casting;
    std::vector<std::pair<Explosion *, Span>> shares;
    uint64_t seenSerial = mOwner.getActorSetSerial();
    size_t first = 0;
    while (first < explosions.size()) {
        round.clear();
        const size_t scanEnd = std::min(explosions.size(), first + APART_LOOKAHEAD);
        for (size_t index = first; index < scanEnd && round.size() < APART_ROUND; ++index) {
            if (ended[index] != 0)
                continue;
            bool apart = true;
            for (const size_t other: round) {
                if (std::abs(cells[index].x - cells[other].x) < APART_BLOCKS
                    && std::abs(cells[index].y - cells[other].y) < APART_BLOCKS
                    && std::abs(cells[index].z - cells[other].z) < APART_BLOCKS) {
                    apart = false;
                    break;
                }
            }
            if (apart)
                round.push_back(index);
        }

        // the rays of the round, in slices, and every share of the actors in reach, handed out together
        casting.clear();
        shares.clear();
        for (const size_t index: round) {
            Explosion &explosion = *explosions[index];
            explosion._beginApart(mApartField);
            if (explosion.mApart != Explosion::Apart::Done)
                continue;
            if (explosion.mApartAny)
                casting.push_back(&explosion);
            collectSpans(level, explosion.mSource, (float) (explosion.mSize * 2.0), mSpans);
            for (const Span &span: mSpans)
                shares.emplace_back(&explosion, span);
        }
        const size_t slices = (size_t) Explosion::APART_SLICES;
        const size_t rays = casting.size() * slices;
        // a share is a few actors, settled in a microsecond: they are taken a handful at a time
        const size_t handfuls = (shares.size() + APART_HANDFUL - 1) / APART_HANDFUL;
        const auto work = [&](size_t slot) {
            if (slot < rays) {
                casting[slot / slices]->_castApart(mApartField, (int) (slot % slices));
                return;
            }
            const size_t first = (slot - rays) * APART_HANDFUL;
            const size_t last = std::min(shares.size(), first + APART_HANDFUL);
            for (size_t share = first; share < last; ++share)
                shares[share].first->_settleApart(mApartField, shares[share].second);
        };
        if (rays + shares.size() < APART_SHARES_ALONE)
            for (size_t slot = 0; slot < rays + handfuls; ++slot)
                work(slot);
        else
            BlastWorkers::get().run(rays + handfuls, work);
        for (Explosion *explosion: casting)
            explosion->_markApart(mApartField);
        for (const size_t index: round) {
            explosions[index]->_finishApart();
            ended[index] = 1;
        }

        // an actor came or went, or a block appeared: what the next round reads is mended first
        // broken blocks open the view of the next explosions: the table of clear boxes is made again once
        // enough actors were settled against the old one to pay for it
        sharesSinceSum += shares.size();
        if (mApartStale || sharesSinceSum >= sumEvery) {
            mApartField.resum();
            mApartStale = false;
            sharesSinceSum = 0;
        }
        if (mOwner.getActorSetSerial() != seenSerial) {
            _syncTargets(level);
            seenSerial = mOwner.getActorSetSerial();
        }
        while (first < explosions.size() && ended[first] != 0)
            ++first;
    }
}
