#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/LavaBlock.h"
#include "World/Level/Block/LiquidBlock.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    // LevelEvent::SoundFizz
    constexpr int32_t SOUND_FIZZ = 1004;

    // the four sides in the order LiquidBlock::_tick walks them for spreading and slope search, with the face
    // liquid enters the neighbour through
    struct Side {
        int32_t x;
        int32_t z;
        int face;
    };

    const Side SPREAD_SIDES[4] = {
            {-1, 0, LiquidFace::EAST}, {1, 0, LiquidFace::WEST}, {0, -1, LiquidFace::SOUTH}, {0, 1, LiquidFace::NORTH}};

    // the order _tick reads neighbour depths in: west, east, south, north
    const Side FEED_SIDES[4] = {
            {-1, 0, LiquidFace::EAST}, {1, 0, LiquidFace::WEST}, {0, 1, LiquidFace::NORTH}, {0, -1, LiquidFace::SOUTH}};

    Vector3i offset(const Vector3i &position, int32_t x, int32_t y, int32_t z) {
        return Vector3i(position.x + x, position.y + y, position.z + z);
    }

    bool isDynamicLiquid(const BlockState &state) {
        return LiquidProperties::of(state).dynamic;
    }

    const BlockState &air() {
        static const BlockState state;
        return state;
    }
}

void LiquidPhysicsSystem::moveStateFrom(LiquidPhysicsSystem &&other) {
    if (this == &other)
        return;
    mTicks.moveStateFrom(std::move(other.mTicks));
    mRandomTicks.moveStateFrom(std::move(other.mRandomTicks));
    mChanges = std::move(other.mChanges);
}

const BlockState &LiquidPhysicsSystem::_stateAt(int32_t x, int32_t y, int32_t z, int layer) {
    static const BlockState bedrock("minecraft:bedrock");

    const LevelChunk *chunk = y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y ? nullptr : _chunkAt(x >> 4, z >> 4);
    // unloaded: a solid wall on layer 0; fluids never flow into or decide from non-resident terrain
    if (chunk == nullptr)
        return layer <= 0 ? bedrock : air();
    return chunk->getBlock(x & 15, y, z & 15, layer);
}

// one tick reads the same few chunks thousands of times
LevelChunk *LiquidPhysicsSystem::_chunkAt(int32_t chunkX, int32_t chunkZ) {
    for (ChunkSlot &slot: mChunkCache) {
        if (slot.mChunk != nullptr && slot.mX == chunkX && slot.mZ == chunkZ)
            return slot.mChunk;
    }

    LevelChunk *chunk = mLevel.peekChunkPtr(chunkX, chunkZ);
    if (chunk == nullptr)
        return nullptr;
    mChunkCache[mChunkCacheNext] = ChunkSlot{chunkX, chunkZ, chunk};
    mChunkCacheNext = (mChunkCacheNext + 1) % CHUNK_CACHE_SIZE;
    return chunk;
}

void LiquidPhysicsSystem::_enter() {
    if (mDepth++ == 0)
        _forgetChunks();
}

void LiquidPhysicsSystem::_forgetChunks() {
    mChunkCache.fill(ChunkSlot{});
}

LiquidProperties LiquidPhysicsSystem::_propertiesAt(const Vector3i &position, int layer) {
    if (!mSearching)
        return LiquidProperties::of(_stateAt(position, layer));

    // the slope search reads the same few blocks hundreds of times and writes nothing
    const int32_t dx = position.x - mSearchOrigin.x + SEARCH_RADIUS;
    const int32_t dy = mSearchOrigin.y - position.y;
    const int32_t dz = position.z - mSearchOrigin.z + SEARCH_RADIUS;
    if (dx < 0 || dx >= SEARCH_SIDE || dz < 0 || dz >= SEARCH_SIDE || dy < 0 || dy > 1)
        return LiquidProperties::of(_stateAt(position, layer));

    const size_t index = ((size_t) (dy * SEARCH_SIDE + dx) * SEARCH_SIDE + (size_t) dz) * 2 + (size_t) layer;
    SearchSlot &slot = mSearchCache[index];
    if (slot.mStamp != mSearchStamp) {
        slot.mProperties = LiquidProperties::of(_stateAt(position, layer));
        slot.mStamp = mSearchStamp;
    }
    return slot.mProperties;
}

LiquidProperties LiquidPhysicsSystem::_liquidPropertiesAt(const Vector3i &position) {
    const LiquidProperties layer0 = _propertiesAt(position);
    if (layer0.isLiquid() || layer0.air)
        return layer0;
    const LiquidProperties layer1 = _propertiesAt(position, 1);
    return layer1.isLiquid() ? layer1 : layer0;
}

// BlockSource::getLiquidBlock: layer 0 unless water sits on layer 1
LiquidPhysicsSystem::Liquid LiquidPhysicsSystem::_liquidAt(const Vector3i &position) {
    const BlockState &layer0 = _stateAt(position);
    const LiquidProperties properties0 = LiquidProperties::of(layer0);
    if (properties0.isLiquid() || properties0.air)
        return Liquid{&layer0, properties0, 0};

    const BlockState &layer1 = _stateAt(position, 1);
    const LiquidProperties properties1 = LiquidProperties::of(layer1);
    if (properties1.isLiquid())
        return Liquid{&layer1, properties1, 1};
    return Liquid{&layer0, properties0, 0};
}

// sub_149C40140: -1 for anything but this liquid
int LiquidPhysicsSystem::_depth(const Vector3i &position, LiquidMaterial material) {
    const LiquidProperties liquid = _liquidPropertiesAt(position);
    return liquid.material == material ? liquid.depth : -1;
}

bool LiquidPhysicsSystem::_isLoaded(const Vector3i &position) {
    return _chunkAt(position.x >> 4, position.z >> 4) != nullptr;
}

// chunk states are interned: the class lookup is cached on them
const Block *LiquidPhysicsSystem::_blockOf(const BlockState &state) {
    return VanillaBlocks::fromState(state);
}

const Block *LiquidPhysicsSystem::_stillBlock(LiquidMaterial material) const {
    static const Block *water = VanillaBlocks::fromIdentifier("minecraft:water");
    static const Block *lava = VanillaBlocks::fromIdentifier("minecraft:lava");
    return material == LiquidMaterial::Lava ? lava : water;
}

int32_t LiquidPhysicsSystem::getTickDelay(LiquidMaterial material) const {
    const LiquidBlock *block = dynamic_cast<const LiquidBlock *>(_stillBlock(material));
    return block == nullptr ? 5 : block->getTickDelay(mLevel.getDimensionType());
}

const BlockState &LiquidPhysicsSystem::_stillState(LiquidMaterial material, int depth) {
    static const std::vector<BlockState> states = []() {
        std::vector<BlockState> result;
        for (const char *name: {"minecraft:water", "minecraft:lava"}) {
            for (int value = 0; value < 16; ++value) {
                Tag tag = Tag::ofCompound();
                tag.putInt("liquid_depth", value);
                result.emplace_back(name, tag);
            }
        }
        return result;
    }();
    const size_t base = material == LiquidMaterial::Lava ? 16 : 0;
    return states[base + (size_t) std::max(0, std::min(15, depth))];
}

void LiquidPhysicsSystem::_write(const Vector3i &position, int layer, const BlockState &state, bool notify) {
    if (!_isLoaded(position) || _stateAt(position, layer) == state)
        return;

    mChanges.push_back(LiquidChange{position, (uint32_t) state.getHash(), layer});
    const bool previous = mNotify;
    mNotify = notify;
    if (layer == 0)
        mLevel.setBlockState(position.x, position.y, position.z, state);
    else
        mLevel.setBlockStateAtLayer(position.x, position.y, position.z, layer, state);
    mNotify = previous;
}

bool LiquidPhysicsSystem::allows(const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                                 const Vector3i *source) {
    const bool allowed = BlockChangeSystem::allows(mLevel, position, state, cause, source);
    _forgetChunks();
    return allowed;
}

void LiquidPhysicsSystem::onBlockChanged(const Vector3i &position, int layer, const BlockState &previous) {
    const Scope scope(*this);
    const bool notify = mNotify && !(mQuiet && mDepth == 1);
    const bool requested = mNotify;
    mNotify = true;
    normalizeWaterlogged(position);

    // BlockPlaceEvent fires when the block itself changes, not one of its states
    const BlockState &current = _stateAt(position, layer);
    const Block *placed = _blockOf(current);
    if (placed != _blockOf(previous)) {
        const LiquidProperties after = LiquidProperties::of(current);
        if (after.isLiquid())
            _onPlace(position, after);
        else if (after.ticking && placed != nullptr)
            placed->onTickPlace(*this, position, current);
    }

    // BlockSource::_blockChanged: neighbours hear of writes made with flag 1 (sub_1432CEDE0)
    if (notify)
        _updateNeighbors(position);
    mNotify = requested;
}

void LiquidPhysicsSystem::updateNeighbours(const Vector3i &position) {
    const Scope scope(*this);
    _updateNeighbors(position);
}

// BlockSource::updateNeighborsAt (sub_1432D2940)
void LiquidPhysicsSystem::_updateNeighbors(const Vector3i &position) {
    _neighborChanged(offset(position, -1, 0, 0), position);
    _neighborChanged(offset(position, 1, 0, 0), position);
    _neighborChanged(offset(position, 0, 0, 1), position);
    _neighborChanged(offset(position, 0, 0, -1), position);
    _neighborChanged(offset(position, 0, 1, 0), position);
    _neighborChanged(offset(position, 0, -1, 0), position);
}

// sub_1432D21E0: the block, then the one on layer 1
void LiquidPhysicsSystem::_neighborChanged(const Vector3i &position, const Vector3i &changed) {
    if (position.y < mLevel.getMinY() || position.y > mLevel.getMaxY() || !_isLoaded(position))
        return;

    for (int layer = 0; layer < 2; ++layer) {
        const BlockState &state = _stateAt(position, layer);
        const LiquidProperties properties = LiquidProperties::of(state);
        if (properties.isLiquid()) {
            _liquidNeighborChanged(position, state);
        } else if (properties.ticking) {
            if (const Block *block = _blockOf(state))
                block->onTickNeighbourChanged(*this, position, state, changed);
        }
    }
}

// LiquidBlock::neighborChanged (sub_149C3DEA0), LiquidBlockDynamic's (sub_149C3E0E0)
void LiquidPhysicsSystem::_liquidNeighborChanged(const Vector3i &position, const BlockState &state) {
    const Block *self = _blockOf(state);
    _solidifyAround(position, self);
    const LiquidProperties properties = LiquidProperties::of(state);
    if (properties.dynamic)
        return;

    // the tick only waits if the liquid read back is this block
    if (_blockOf(*_liquidAt(position).state) != self)
        return;

    const int32_t delay = getTickDelay(properties.material);
    if (!mTicks.isPending(position, self, delay))
        mTicks.add(position, self, delay);
}

// LiquidBlockBase and LiquidBlockDynamic on BlockPlaceEvent (sub_149C40D30, sub_149C42D00)
void LiquidPhysicsSystem::_onPlace(const Vector3i &position, const LiquidProperties &placed) {
    const Liquid liquid = _liquidAt(position);
    const Block *self = _blockOf(*liquid.state);
    _solidifyAround(position, self);
    if (!placed.dynamic || _blockOf(*_liquidAt(position).state) != self)
        return;

    // flowing blocks never last: still block with the same depth, written without neighbour updates
    _write(position, liquid.layer, _stillState(placed.material, placed.depth), false);
    mTicks.add(position, _stillBlock(placed.material), getTickDelay(placed.material));
}

void LiquidPhysicsSystem::_solidifyAround(const Vector3i &position, const Block *self) {
    _solidify(position, offset(position, 0, 0, 1), self);
    _solidify(position, offset(position, 0, 0, -1), self);
    _solidify(position, offset(position, 1, 0, 0), self);
    _solidify(position, offset(position, -1, 0, 0), self);
    _solidify(position, offset(position, 0, 1, 0), self);
}

// sub_149C41E60 queues the check while one runs; sub_149C42400 acts only for the lava instance that asked
void LiquidPhysicsSystem::_solidify(const Vector3i &position, const Vector3i &neighbor, const Block *self) {
    if (mSolidifying) {
        mDeferredSolidify.push_back(SolidifyCheck{position, neighbor, self});
        return;
    }

    static const BlockState obsidian("minecraft:obsidian");
    static const BlockState cobblestone("minecraft:cobblestone");
    static const BlockState basalt("minecraft:basalt");
    static const Block *blueIce = VanillaBlocks::fromIdentifier("minecraft:blue_ice");
    static const Block *soulSoil = VanillaBlocks::fromIdentifier("minecraft:soul_soil");

    mSolidifying = true;
    SolidifyCheck next{position, neighbor, self};
    while (true) {
        const BlockState &lavaState = _stateAt(next.position);
        const LiquidProperties lava = LiquidProperties::of(lavaState);
        if (lava.material == LiquidMaterial::Lava && _blockOf(lavaState) == next.self
            && next.neighbor.y >= next.position.y) {
            const BlockState *result = nullptr;
            if (_liquidAt(next.neighbor).properties.material == LiquidMaterial::Water)
                result = lava.depth == 0 ? &obsidian : &cobblestone;
            else if (_blockOf(_stateAt(next.neighbor)) == blueIce
                     && _blockOf(_stateAt(offset(next.position, 0, -1, 0))) == soulSoil)
                result = &basalt;

            if (result != nullptr && allows(next.position, *result, BlockChangeCause::Form)) {
                _write(next.position, 0, *result, true);
                _fizz(next.position);
            }
        }

        if (mDeferredSolidify.empty())
            break;
        next = mDeferredSolidify.front();
        mDeferredSolidify.pop_front();
    }
    mSolidifying = false;
}

// sub_149C3FF00: the sound only, the particles are client side
void LiquidPhysicsSystem::_fizz(const Vector3i &position) {
    ServerNetworkHandler *owner = mLevel.getOwner();
    if (owner == nullptr)
        return;

    const float first = (float) RandomTickSystem::nextInt(1 << 24) / (float) (1 << 24);
    const float second = (float) RandomTickSystem::nextInt(1 << 24) / (float) (1 << 24);
    const int32_t pitch = (int32_t) (((first - second) * 0.8f + 2.6f) * 1000.0f);
    owner->broadcastLevelEvent(
            mLevel, SOUND_FIZZ,
            Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f), pitch);
}

void LiquidPhysicsSystem::schedule(const Vector3i &position, int64_t delay) {
    const Scope scope(*this);
    const Liquid liquid = _liquidAt(position);
    const Block *block = _blockOf(*liquid.state);
    if (block != nullptr && (liquid.properties.isLiquid() || liquid.properties.ticking))
        mTicks.add(position, block, (int32_t) delay);
}

void LiquidPhysicsSystem::queueTick(const Vector3i &position, const Block *block, int32_t delay) {
    mTicks.add(position, block, delay);
}

void LiquidPhysicsSystem::queueRandomTick(const Vector3i &position, const Block *block, int32_t delay) {
    mRandomTicks.add(position, block, delay);
}

// LevelChunk::tick (sub_143854100): the first queue, then the second on its own clock, chunk by chunk
// no early out when nothing is queued: every ticked chunk's clock advances each tick (sub_14381B7E0)
void LiquidPhysicsSystem::tick(int64_t levelTick, const std::vector<int64_t> &columns) {
    const Scope scope(*this);
    const auto loaded = [this](int32_t chunkX, int32_t chunkZ) { return _chunkAt(chunkX, chunkZ) != nullptr; };
    const auto run = [this](const Vector3i &position, const Block *block) { _onTick(position, block); };
    mTicks.beginTick(levelTick);
    mRandomTicks.beginTick(levelTick, mLevel.getGameRules().getInt("randomtickspeed"));
    for (int64_t column: columns) {
        mTicks.tickColumn(levelTick, column, run, loaded);
        mRandomTicks.tickColumn(levelTick, column, run, loaded);
    }
    mTicks.endTick();
    mRandomTicks.endTick();
}

// the tick fires for the block it was queued with, on either layer (sub_14381B7E0, sub_143166CE0)
void LiquidPhysicsSystem::_onTick(const Vector3i &position, const Block *block) {
    for (int layer = 0; layer < 2; ++layer) {
        const BlockState &state = _stateAt(position, layer);
        if (_blockOf(state) != block)
            continue;

        const LiquidProperties properties = LiquidProperties::of(state);
        if (properties.isLiquid()) {
            // LiquidBlockDynamic on BlockQueuedTickEvent (sub_149C43290)
            if (properties.dynamic)
                _write(position, layer, _stillState(properties.material, properties.depth), false);
            _tickLiquid(position, properties.material);
        } else if (properties.ticking) {
            block->onQueuedTick(*this, position, state);
        }
    }
}

// LiquidBlock::_isLiquidBlocking (sub_149C40280); `face` is the one liquid enters `target` through
bool LiquidPhysicsSystem::_isLiquidBlocking(const Vector3i &target, const Vector3i &from, int face) {
    const LiquidProperties blocker = _propertiesAt(target);
    if (_liquidPropertiesAt(from).material == LiquidMaterial::Lava)
        return blocker.lavaBlocking;
    if (blocker.reaction == LiquidReaction::Blocking || (blocker.blockedFaces >> face & 1) != 0)
        return true;
    return (_propertiesAt(from).blockedFaces >> LiquidFace::opposite(face) & 1) != 0;
}

// Block::spawnResources of a block water washes away
void LiquidPhysicsSystem::_popResources(const Vector3i &position, const BlockState &state) {
    ServerNetworkHandler *owner = mLevel.getOwner();
    if (owner == nullptr || !mLevel.getGameRules().getBool("dotiledrops"))
        return;
    BlockActionHandler::spawnBlockDrops(*owner, mLevel, position, state, ItemStack::air());
    _forgetChunks();
}

// LiquidBlock::_trySpreadTo (sub_149C3F9B0)
void LiquidPhysicsSystem::_trySpreadTo(LiquidMaterial self, const Vector3i &target, int depth, const Vector3i &from,
                                       int face) {
    if (target.y < mLevel.getMinY() || target.y > mLevel.getMaxY() || !_isLoaded(target))
        return;

    const LiquidMaterial present = _liquidAt(target).properties.material;
    if (present == LiquidMaterial::Lava || present == self || _isLiquidBlocking(target, from, face))
        return;

    const BlockState &flowing = _stillState(self, depth);
    const bool allowed = BlockChangeSystem::allowsFlow(mLevel, from, target, flowing);
    _forgetChunks();
    if (!allowed)
        return;

    bool waterlog = false;
    // interned: still valid once drops have run
    const BlockState &layer0 = _stateAt(target);
    const LiquidProperties replaced = LiquidProperties::of(layer0);
    if (!replaced.air) {
        if (self == LiquidMaterial::Lava)
            _fizz(target);
        else if (replaced.reaction == LiquidReaction::Popped)
            _popResources(target, layer0);
        else
            waterlog = replaced.reaction == LiquidReaction::NoReaction;

        const BlockState &layer1 = _stateAt(target, 1);
        const LiquidProperties extra = LiquidProperties::of(layer1);
        if (extra.reaction == LiquidReaction::Popped && !extra.air)
            _popResources(target, layer1);
    }

    _write(target, waterlog ? 1 : 0, flowing, true);
    mTicks.add(target, _stillBlock(self), getTickDelay(self));
}

// LiquidBlock::_getSlopeDistance (sub_149C40440); `from` is the side index walked in, never walked back
int LiquidPhysicsSystem::_slopeDistance(LiquidMaterial self, const Vector3i &position, int pass, int from) {
    // pure while the search runs: walks meeting at a block with the same pass and side share the answer
    const int32_t dx = position.x - mSearchOrigin.x + SEARCH_RADIUS;
    const int32_t dz = position.z - mSearchOrigin.z + SEARCH_RADIUS;
    SlopeSlot *memo = nullptr;
    if (dx >= 0 && dx < SEARCH_SIDE && dz >= 0 && dz < SEARCH_SIDE && pass >= 0 && pass < SLOPE_PASSES) {
        memo = &mSlopeCache[(((size_t) dx * SEARCH_SIDE + (size_t) dz) * SLOPE_PASSES + (size_t) pass) * 4
                            + (size_t) from];
        if (memo->mStamp == mSearchStamp)
            return memo->mCost;
    }

    const int cost = _searchSlope(self, position, pass, from);
    if (memo != nullptr) {
        memo->mStamp = mSearchStamp;
        memo->mCost = (int16_t) cost;
    }
    return cost;
}

int LiquidPhysicsSystem::_searchSlope(LiquidMaterial self, const Vector3i &position, int pass, int from) {
    const int maxPass = self == LiquidMaterial::Lava ? 2 : 4;
    int cost = 1000;
    for (int side = 0; side < 4; ++side) {
        if (side == (from ^ 1))
            continue;

        const Vector3i next = offset(position, SPREAD_SIDES[side].x, 0, SPREAD_SIDES[side].z);
        if (_isLiquidBlocking(next, position, SPREAD_SIDES[side].face))
            continue;

        const LiquidProperties liquid = _liquidPropertiesAt(next);
        if (liquid.material == self && liquid.depth == 0)
            continue;

        if (!_isLiquidBlocking(offset(next, 0, -1, 0), position, LiquidFace::UP))
            return pass;

        if (pass < maxPass)
            cost = std::min(cost, _slopeDistance(self, next, pass + 1, side));
    }
    return cost;
}

// LiquidBlock::_tick (sub_149C3E1F0)
void LiquidPhysicsSystem::_tickLiquid(const Vector3i &position, LiquidMaterial self) {
    static const BlockState obsidian("minecraft:obsidian");
    static const BlockState stone("minecraft:stone");

    const bool lava = self == LiquidMaterial::Lava;
    const bool ultrawarm = mLevel.getDimensionType() == DimensionType::Nether;
    const int dropOff = lava && !ultrawarm ? 2 : 1;
    int delay = getTickDelay(self);
    const Vector3i below = offset(position, 0, -1, 0);
    const Vector3i above = offset(position, 0, 1, 0);

    if (lava)
        LavaBlock::spreadFire(*this, position);

    int depth = _depth(position, self);
    if (depth < 0)
        return;

    bool unchanged = true;
    if (depth > 0) {
        int highest = -100;
        int sources = 0;
        for (const Side &side: FEED_SIDES) {
            const Vector3i neighbor = offset(position, side.x, 0, side.z);
            // the +185 bit and vtable+136 the game tests are one measured mask here
            if ((_propertiesAt(neighbor).blockedFaces >> side.face & 1) != 0)
                continue;
            const int neighborDepth = _depth(neighbor, self);
            if (neighborDepth < 0)
                continue;
            if (neighborDepth == 0)
                ++sources;
            const int level = neighborDepth >= 8 ? 0 : neighborDepth;
            if (highest < 0 || level < highest)
                highest = level;
        }

        int next = highest < 0 || highest + dropOff >= 8 ? -1 : highest + dropOff;
        const int aboveDepth = _depth(above, self);
        if (aboveDepth >= 0)
            next = aboveDepth >= 8 ? aboveDepth : aboveDepth + 8;

        // a source above counts with the side sources
        if (!lava && (sources >= 2 || (sources == 1 && aboveDepth == 0))) {
            const LiquidProperties floor = _propertiesAt(below);
            const LiquidProperties underneath = _liquidAt(below).properties;
            if (floor.solid || (underneath.material == self && underneath.depth == 0))
                next = 0;
        }

        // drying lava keeps its pace three times out of four
        if (lava && depth <= 7 && next <= 7 && next > depth && RandomTickSystem::nextInt(4) != 0)
            delay *= 4;

        if (next != depth) {
            unchanged = false;
            const int layer = _liquidAt(position).layer;
            if (next < 0) {
                _write(position, layer, air(), true);
            } else {
                _write(position, layer, _stillState(self, next), true);
                // the game updates the neighbours a second time after the write
                _updateNeighbors(position);
                mTicks.add(position, _stillBlock(self), delay);
            }
            depth = next;
        }
    }

    if (unchanged) {
        // still lava source under water turns to obsidian without telling its neighbours
        const Liquid current = _liquidAt(position);
        if (current.properties.material == LiquidMaterial::Lava && !current.properties.dynamic
            && current.properties.depth == 0 && LiquidProperties::of(_stateAt(above)).material == LiquidMaterial::Water
            && allows(position, obsidian, BlockChangeCause::Form))
            _write(position, 0, obsidian, false);
    }

    if (below.y >= mLevel.getMinY() && _isLoaded(below)) {
        const LiquidMaterial underneath = _liquidAt(below).properties.material;
        if (underneath != LiquidMaterial::Lava && underneath != self
            && !_isLiquidBlocking(below, position, LiquidFace::UP)) {
            // lava onto water, or anything onto a block of material 18, makes stone
            const Block *floor = _blockOf(_stateAt(below));
            if ((lava && underneath == LiquidMaterial::Water) || (floor != nullptr && floor->isIceMaterial())) {
                if (allows(below, stone, BlockChangeCause::Form)) {
                    _write(below, 0, stone, true);
                    _fizz(below);
                }
                return;
            }
            _trySpreadTo(self, below, depth < 8 ? depth + 8 : depth, position, LiquidFace::UP);
            return;
        }
    }

    if (depth < 0 || (depth != 0 && !_isLiquidBlocking(below, position, LiquidFace::UP)))
        return;

    int start = 1;
    const Liquid current = _liquidAt(position);
    if (current.properties.material == self && current.properties.depth >= 5 && current.properties.depth <= 7)
        start = current.properties.depth - 3;

    int costs[4];
    mSearching = true;
    mSearchOrigin = position;
    ++mSearchStamp;
    for (int side = 0; side < 4; ++side) {
        costs[side] = 1000;
        const Vector3i neighbor = offset(position, SPREAD_SIDES[side].x, 0, SPREAD_SIDES[side].z);
        if (_isLiquidBlocking(neighbor, position, SPREAD_SIDES[side].face))
            continue;
        const LiquidProperties liquid = _liquidPropertiesAt(neighbor);
        if (liquid.material == self && liquid.depth == 0)
            continue;
        costs[side] = _isLiquidBlocking(offset(neighbor, 0, -1, 0), position, LiquidFace::UP)
                              ? _slopeDistance(self, neighbor, start, side)
                              : 0;
    }
    mSearching = false;

    const int spread = depth < 8 ? depth + dropOff : 1;
    if (spread > 7)
        return;

    const int lowest = *std::min_element(costs, costs + 4);
    for (int side = 0; side < 4; ++side) {
        if (costs[side] != lowest)
            continue;
        _trySpreadTo(self, offset(position, SPREAD_SIDES[side].x, 0, SPREAD_SIDES[side].z), spread, position,
                     SPREAD_SIDES[side].face);
    }
}

const BlockState &LiquidPhysicsSystem::fluidStateAt(const LevelChunk &chunk, int32_t localX, int32_t y,
                                                    int32_t localZ) {
    const BlockState &layer0 = chunk.getBlock(localX, y, localZ);
    const LiquidProperties properties = LiquidProperties::of(layer0);
    if (properties.isLiquid() || !properties.containsWater)
        return layer0;

    const BlockState &layer1 = chunk.getBlock(localX, y, localZ, 1);
    return LiquidView(layer1).isWater() ? layer1 : layer0;
}

uint8_t LiquidPhysicsSystem::getWaterloggingLevel(const BlockState &state) {
    const LiquidProperties properties = LiquidProperties::of(state);
    if (!properties.containsWater || properties.bubbleColumn)
        return 0;
    return properties.reaction == LiquidReaction::NoReaction ? 2 : 1;
}

// a block that cannot hold water drops its layer 1; water left alone in air moves down to layer 0
void LiquidPhysicsSystem::normalizeWaterlogged(const Vector3i &position) {
    const BlockState &layer1 = _stateAt(position, 1);
    if (!LiquidProperties::of(layer1).isLiquid())
        return;

    const LiquidProperties layer0 = _propertiesAt(position);
    if (layer0.air) {
        // interned: still the water once layer 1 is cleared
        const BlockState &water = layer1;
        _write(position, 1, air(), false);
        _write(position, 0, water, false);
        return;
    }
    // a column's water moves to layer 1 under a water block for an instant (sub_1435B5770)
    if (!layer0.containsWater && layer0.material != LiquidMaterial::Water)
        _write(position, 1, air(), false);
}

std::vector<LiquidChange> LiquidPhysicsSystem::consumeChanges() {
    std::vector<LiquidChange> result;
    result.swap(mChanges);
    return result;
}

std::string LiquidPhysicsSystem::_saveRecord(const BlockTickingQueue &queue, int32_t chunkX, int32_t chunkZ) const {
    const std::vector<BlockTickingQueue::Entry> *entries = queue.getColumnEntries(chunkX, chunkZ);
    if (entries == nullptr)
        return std::string();

    std::vector<Tag> list;
    for (const BlockTickingQueue::Entry &entry: *entries) {
        Tag tick = Tag::ofCompound();
        tick.putInt("x", entry.x);
        tick.putInt("y", entry.y);
        tick.putInt("z", entry.z);
        tick.put("blockState", entry.block->toBlockState().toNbt());
        tick.putLong("time", entry.due);
        list.push_back(tick);
    }

    Tag record = Tag::ofCompound();
    record.put("tickList", Tag::ofList(Tag::Type::Compound, list));
    record.putInt("currentTick", (int32_t) queue.getColumnTick(chunkX, chunkZ));
    BinaryStream stream;
    NbtIo::writeTag(stream, record, NbtVariant::LittleEndian);
    return stream.getBuffer();
}

void LiquidPhysicsSystem::_loadRecord(BlockTickingQueue &queue, int32_t chunkX, int32_t chunkZ,
                                      const std::string &record) {
    if (record.empty())
        return;

    Tag root;
    try {
        ReadOnlyBinaryStream stream(record);
        root = NbtIo::readTag(stream, NbtVariant::LittleEndian);
    } catch (const std::exception &) {
        return;
    }

    const Tag *list = root.get("tickList");
    if (list == nullptr || list->getType() != Tag::Type::List)
        return;

    std::vector<BlockTickingQueue::Entry> entries;
    for (const Tag &tick: list->getList()) {
        const Tag *state = tick.get("blockState");
        if (state == nullptr)
            continue;
        const Block *block = VanillaBlocks::fromIdentifier(state->getString("name"));
        if (block == nullptr)
            continue;
        entries.push_back(BlockTickingQueue::Entry{tick.getInt("x"), tick.getInt("y"), tick.getInt("z"), 0,
                                                   tick.getLong("time"), block});
    }
    queue.loadColumn(chunkX, chunkZ, root.getInt("currentTick"), entries);
}

// PendingTicks (sub_14381D090, sub_14381C610) and RandomTicks, the same record for the second queue
void LiquidPhysicsSystem::loadPendingTicks(LevelChunk &chunk) {
    _loadRecord(mTicks, chunk.getX(), chunk.getZ(), chunk.getPendingTicks());
    _loadRecord(mRandomTicks, chunk.getX(), chunk.getZ(), chunk.getRandomTicks());
    if (chunk.needsLiquidMigration())
        _migrate(chunk);
}

bool LiquidPhysicsSystem::savePendingTicks(LevelChunk &chunk) const {
    std::string pending = _saveRecord(mTicks, chunk.getX(), chunk.getZ());
    std::string random = _saveRecord(mRandomTicks, chunk.getX(), chunk.getZ());
    if (pending == chunk.getPendingTicks() && random == chunk.getRandomTicks())
        return false;
    chunk.setPendingTicks(std::move(pending));
    chunk.setRandomTicks(std::move(random));
    return true;
}

// Falcon before data version 2 saved flowing blocks, falling ones at depth 0: the game keeps only still blocks,
// falling at depth 8, each waiting for its tick
void LiquidPhysicsSystem::_migrate(LevelChunk &chunk) {
    chunk.setNeedsLiquidMigration(false);
    for (int index = 0; index < LevelChunk::SUB_CHUNK_COUNT; ++index) {
        SubChunk &section = chunk.getSubChunk(index);
        if (!section.paletteMatches(SubChunk::PaletteQuery::DynamicLiquid, isDynamicLiquid))
            continue;

        const int32_t baseY = (LevelChunk::LOWEST_SUB_CHUNK_Y + index) * 16;
        for (int layer = 0; layer < SubChunk::LAYER_COUNT; ++layer) {
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 0; y < 16; ++y) {
                        const LiquidProperties liquid = LiquidProperties::of(section.getBlock(x, y, z, layer));
                        if (!liquid.dynamic)
                            continue;
                        const int depth = liquid.depth == 0 ? 8 : liquid.depth;
                        section.setBlock(x, y, z, layer, _stillState(liquid.material, depth));
                        const Vector3i position(chunk.getX() * 16 + x, baseY + y, chunk.getZ() * 16 + z);
                        mTicks.add(position, _stillBlock(liquid.material), getTickDelay(liquid.material));
                    }
                }
            }
        }
    }
    chunk.markDirty();
}

LiquidInfo LiquidPhysicsSystem::getLiquidInfo(int32_t x, int32_t y, int32_t z) {
    const Scope scope(*this);
    LiquidInfo result;
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return result;

    const Vector3i position(x, y, z);
    const BlockState &layer0 = _stateAt(position);
    const LiquidView block(LiquidView(layer0).isBubbleColumn() ? layer0 : *_liquidAt(position).state);
    result.water = block.isWater();
    result.lava = block.isLava();
    result.bubble = block.isBubbleColumn();
    result.dragDown = result.bubble && block.isDragDown();
    result.source = block.isSource();
    result.falling = block.isFalling();
    result.decay = block.getDecay();
    result.height = block.isLiquid() || result.bubble ? block.getFluidHeightPercent() : 0.0f;
    return result;
}

bool LiquidPhysicsSystem::_isFlowable(const BlockState &state, bool lava) const {
    const LiquidProperties properties = LiquidProperties::of(state);
    if (properties.air || properties.isLiquid())
        return true;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return false;

    // waterlogging level 1: source only, no flowing water; higher levels take flowing water into layer 1;
    // lava never waterlogs
    const uint8_t waterlogging = getWaterloggingLevel(state);
    if (!lava && waterlogging > 1)
        return true;
    if (!lava && waterlogging == 1)
        return false;
    return !data->mSolid;
}

Vector3f LiquidPhysicsSystem::getFlowVector(const Vector3i &position) {
    const Scope scope(*this);
    const LiquidView liquid(*_liquidAt(position).state);
    if (!liquid.isLiquid())
        return Vector3f();

    const bool lava = liquid.isLava();
    const int decay = liquid.isSource() || liquid.isFalling() ? 0 : liquid.getDecay();
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    static const int offsets[4][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    // pushes towards higher decay (shallower fluid); an open side over fluid counts 8 levels deeper:
    // entities are pulled over edges
    for (const auto &side: offsets) {
        const Vector3i next = offset(position, side[0], 0, side[2]);
        const BlockState &sideState = *_liquidAt(next).state;
        const LiquidView sideLiquid(sideState);
        if (sideLiquid.isLiquid() && sideLiquid.isLava() == lava) {
            const int sideDecay = sideLiquid.isSource() || sideLiquid.isFalling() ? 0 : sideLiquid.getDecay();
            const int realDecay = sideDecay - decay;
            x += (float) side[0] * (float) realDecay;
            z += (float) side[2] * (float) realDecay;
        } else if (_isFlowable(sideState, lava)) {
            const LiquidView belowLiquid(*_liquidAt(offset(next, 0, -1, 0)).state);
            if (belowLiquid.isLiquid() && belowLiquid.isLava() == lava) {
                const int belowDecay = belowLiquid.isSource() || belowLiquid.isFalling() ? 0 : belowLiquid.getDecay();
                const int realDecay = belowDecay - (decay - 8);
                x += (float) side[0] * (float) realDecay;
                z += (float) side[2] * (float) realDecay;
            }
        }
    }

    // falling fluid next to a wall pulls down hard: drags entities along waterfalls
    if (liquid.isFalling()) {
        for (const auto &side: offsets) {
            const Vector3i next = offset(position, side[0], 0, side[2]);
            if (!_isFlowable(*_liquidAt(next).state, lava)
                || !_isFlowable(*_liquidAt(offset(next, 0, 1, 0)).state, lava)) {
                const float length = std::sqrt(x * x + y * y + z * z);
                if (length > 0.0f) {
                    x /= length;
                    z /= length;
                }
                y -= 6.0f;
                break;
            }
        }
    }

    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.0f)
        return Vector3f();
    return Vector3f(x / length, y / length, z / length);
}
