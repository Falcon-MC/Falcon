#include "World/Level/Block/Systems/RandomTickSystem.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/CopperSystem.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/SubChunk.h"
#include "World/Level/Block/BlockNames.h"

#include <array>
#include <bit>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
    // most sampled blocks (stone, dirt, ores) react to nothing: dropped before the candidate copy;
    // copper weathering runs through CopperSystem, not the block class
    bool ticksRandomly(const Block &block) {
        static std::unordered_map<const Block *, bool> cache;
        const auto found = cache.find(&block);
        if (found != cache.end())
            return found->second;

        const bool ticks = block.hasRandomTick() || !CopperSystem::oxidizedOf(block.getIdentifier()).empty();
        cache.emplace(&block, ticks);
        return ticks;
    }

    // a pack block has no class: it ticks for the scripts' onRandomTick
    bool isScriptedBlock(const BlockState &state) {
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
        return custom != nullptr && (!custom->mCustomComponents.empty() || !custom->mPermutations.empty());
    }

    struct TickBlock {
        const Block *mBlock = nullptr;
        bool mEligible = false;
    };

    TickBlock classify(const BlockState &state) {
        if (state.mName == BlockNames::AIR)
            return {};

        const Block *block = VanillaBlocks::fromState(state);
        return {block, block != nullptr ? ticksRandomly(*block) : isScriptedBlock(state)};
    }

    bool stateTicksRandomly(const BlockState &state) {
        return classify(state).mEligible;
    }

    // Collection runs before callbacks. Interned states recur across chunks and their classification
    // is stable throughout this pass, including when several packs provide custom blocks.
    struct TickClassification {
        const BlockState *mState = nullptr;
        TickBlock mResult;
    };

    struct Candidate {
        Vector3i mPosition;
        const Block *mBlock;
        // set for a pack block, mBlock is null then
        std::string_view mCustom;
    };

    std::mt19937 &generator() {
        static std::mt19937 random(std::random_device{}());
        return random;
    }

    constexpr uint32_t UPDATE_INCREMENT = 0x3c6ef35f;

    uint32_t &updateState() {
        static uint32_t state = UPDATE_INCREMENT;
        return state;
    }

    struct LcgStep {
        uint32_t mMultiplier = 1;
        uint32_t mIncrement = 0;

        uint32_t apply(uint32_t value) const {
            return value * mMultiplier + mIncrement;
        }

        LcgStep then(const LcgStep &next) const {
            return {mMultiplier * next.mMultiplier, next.apply(mIncrement)};
        }
    };

    // each inert nonempty section still advances the same generator, including at high randomtickspeed
    std::array<LcgStep, LevelChunk::SUB_CHUNK_COUNT + 1> skippedSections(uint32_t speed) {
        LcgStep step;
        LcgStep power{3, UPDATE_INCREMENT};
        for (; speed != 0; speed >>= 1) {
            if ((speed & 1) != 0)
                step = step.then(power);
            power = power.then(power);
        }

        std::array<LcgStep, LevelChunk::SUB_CHUNK_COUNT + 1> skips{};
        for (size_t i = 1; i < skips.size(); ++i)
            skips[i] = skips[i - 1].then(step);
        return skips;
    }
}

int RandomTickSystem::nextInt(int bound) {
    if (bound <= 1)
        return 0;

    std::uniform_int_distribution<int> distribution(0, bound - 1);
    return distribution(generator());
}

float RandomTickSystem::nextFloat() {
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator());
}

int RandomTickSystem::getFullLight(Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return 0;

    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    if (chunk == nullptr)
        return 0;

    const int localX = position.x & 15;
    const int localZ = position.z & 15;

    const int blockLight = chunk->hasBlockLight() ? chunk->getBlockLight(localX, position.y, localZ) : 0;
    if (!level.hasSkyLight())
        return blockLight;

    const int skyLight = level.getSkyLightAt(position.x, position.y, position.z) - level.getSkyLightSubtracted();
    return skyLight > blockLight ? skyLight : blockLight;
}

int RandomTickSystem::getBlockLight(Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return 0;

    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    if (chunk == nullptr || !chunk->hasBlockLight())
        return 0;

    return chunk->getBlockLight(position.x & 15, position.y, position.z & 15);
}

void RandomTickSystem::tick(ServerNetworkHandler &owner, Level &level) {
    const int32_t speed = level.getGameRules().getInt("randomtickspeed");
    if (speed <= 0)
        return;

    // thousands of picks per tick: reused buffer, block pointer instead of a state copy
    static std::vector<Candidate> candidates;
    candidates.clear();

    std::array<TickClassification, 1024> classifications{};
    const auto skips = skippedSections((uint32_t) speed);
    uint32_t lcgState = updateState();
    const auto &columns = level.getActiveColumnOrder();
    struct ActiveChunk {
        const LevelChunk *mChunk;
        LevelChunk::RandomTickSections mSections;
    };
    std::array<ActiveChunk, 16> batch;
    for (size_t first = 0; first < columns.size(); first += batch.size()) {
        size_t count = 0;
        for (size_t i = first; i < columns.size() && i < first + batch.size(); ++i) {
            const int64_t column = columns[i];
            const LevelChunk *chunk = level.peekChunkPtr((int32_t) (column >> 32), (int32_t) column);
            if (chunk == nullptr)
                continue;
            __builtin_prefetch(chunk);
            batch[count++].mChunk = chunk;
        }

        // Fetch the eligible section headers before drawing samples from their index buffers.
        for (size_t i = 0; i < count; ++i) {
            const LevelChunk &chunk = *batch[i].mChunk;
            batch[i].mSections = chunk.getRandomTickSections(stateTicksRandomly);
            for (uint32_t eligible = batch[i].mSections.mEligible; eligible != 0; eligible &= eligible - 1)
                __builtin_prefetch(&chunk.getSubChunk(std::countr_zero(eligible)));
        }

        for (size_t i = 0; i < count; ++i) {
            const LevelChunk *chunk = batch[i].mChunk;
            const int32_t chunkX = chunk->getX();
            const int32_t chunkZ = chunk->getZ();
            auto sections = batch[i].mSections;
            while (sections.mEligible != 0) {
                const int index = std::countr_zero(sections.mEligible);
                const uint32_t bit = uint32_t{1} << index;
                lcgState = skips[std::popcount(sections.mNonEmpty & (bit - 1))].apply(lcgState);
                sections.mNonEmpty &= ~(bit | (bit - 1));
                sections.mEligible &= ~bit;

                const SubChunk &subChunk = chunk->getSubChunk(index);
                for (int32_t attempt = 0; attempt < speed; attempt++) {
                    lcgState = lcgState * 3 + UPDATE_INCREMENT;
                    const uint32_t lcg = lcgState >> 2;
                    const int localX = (int) (lcg & 0x0f);
                    const int localY = (int) ((lcg >> 8) & 0x0f);
                    const int localZ = (int) ((lcg >> 16) & 0x0f);

                    const BlockState *state = subChunk.getPotentialRandomTickState(localX, localY, localZ);
                    if (state == nullptr)
                        continue;
                    const uintptr_t address = reinterpret_cast<uintptr_t>(state);
                    auto &lookup = classifications[((address >> 4) ^ (address >> 14)) & (classifications.size() - 1)];
                    if (lookup.mState != state)
                        lookup = {state, classify(*state)};
                    if (!lookup.mResult.mEligible)
                        continue;

                    Candidate candidate;
                    candidate.mPosition = Vector3i((chunkX << 4) + localX, LevelChunk::MIN_Y + index * 16 + localY,
                                                   (chunkZ << 4) + localZ);
                    candidate.mBlock = lookup.mResult.mBlock;
                    if (candidate.mBlock == nullptr)
                        candidate.mCustom = state->mName;
                    candidates.push_back(candidate);
                }
            }
            lcgState = skips[std::popcount(sections.mNonEmpty)].apply(lcgState);
        }
    }
    updateState() = lcgState;

    // collect then tick: reactions never modify chunks mid-iteration; re-read, an earlier tick may have
    // replaced it
    for (const Candidate &candidate: candidates) {
        const Vector3i &position = candidate.mPosition;
        const BlockState *before = level.peekBlockPtr(position.x, position.y, position.z);
        if (before == nullptr || VanillaBlocks::fromState(*before) != candidate.mBlock)
            continue;

        if (candidate.mBlock == nullptr) {
            if (before->mName == candidate.mCustom)
                owner.getScriptEngine().onCustomBlockRandomTick(level, before->mName, position);
            continue;
        }

        // palettes point to interned states that outlive block changes
        candidate.mBlock->onRandomTick(owner, level, position, *before);

        if (const BlockState *after = level.peekBlockPtr(position.x, position.y, position.z))
            CopperSystem::onRandomTick(owner, level, position, *after);
    }
}
