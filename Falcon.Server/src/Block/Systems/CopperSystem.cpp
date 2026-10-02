#include "Block/Systems/CopperSystem.h"

#include "Block/BlockActorStore.h"
#include "Block/BlockData.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

#include <cstdlib>
#include <vector>

namespace {
    const std::string PREFIX = "minecraft:";
    const std::string WAXED = "minecraft:waxed_";
    const char *const OXIDATION_STAGES[] = {"exposed_", "weathered_", "oxidized_"};
    const int32_t STAGE_COUNT = 3;
    const int32_t NEIGHBOUR_RADIUS = 4;
    const int32_t OXIDATION_CHANCE_NUMERATOR = 64;
    const int32_t OXIDATION_CHANCE_DENOMINATOR = 1125;
    const int32_t CHANCE_PRECISION = 1000000;
    const float UNAFFECTED_MULTIPLIER = 0.75f;
    const char *UPPER_BLOCK_BIT = "upper_block_bit";

    bool startsWith(const std::string &value, const std::string &prefix) {
        return value.compare(0, prefix.size(), prefix) == 0;
    }

    bool exists(const std::string &identifier) {
        return BlockDataTable::find(identifier.c_str()) != nullptr;
    }

    bool isCopper(const std::string &identifier) {
        return identifier.find("copper") != std::string::npos;
    }

    bool roll(float chance) {
        return RandomTickSystem::nextInt(CHANCE_PRECISION) < (int32_t) (chance * (float) CHANCE_PRECISION);
    }
}

std::string CopperSystem::waxedOf(const std::string &identifier) {
    if (!isCopper(identifier) || startsWith(identifier, WAXED) || !startsWith(identifier, PREFIX))
        return std::string();

    const std::string candidate = WAXED + identifier.substr(PREFIX.size());
    if (exists(candidate))
        return candidate;

    const std::string suffix = "_block";
    if (candidate.size() > suffix.size()
        && candidate.compare(candidate.size() - suffix.size(), suffix.size(), suffix) == 0) {
        const std::string trimmed = candidate.substr(0, candidate.size() - suffix.size());
        if (exists(trimmed))
            return trimmed;
    }

    return std::string();
}

std::string CopperSystem::withoutWaxOf(const std::string &identifier) {
    if (!startsWith(identifier, WAXED))
        return std::string();

    const std::string candidate = PREFIX + identifier.substr(WAXED.size());
    if (exists(candidate))
        return candidate;

    return exists(candidate + "_block") ? candidate + "_block" : std::string();
}

std::string CopperSystem::scrapedOf(const std::string &identifier) {
    for (int32_t stage = 0; stage < STAGE_COUNT; stage++) {
        const std::string prefix = PREFIX + OXIDATION_STAGES[stage];
        if (!startsWith(identifier, prefix))
            continue;

        const std::string rest = identifier.substr(prefix.size());
        std::string candidate = stage == 0 ? PREFIX + rest : PREFIX + OXIDATION_STAGES[stage - 1] + rest;
        if (stage == 0 && !exists(candidate))
            candidate += "_block";

        return exists(candidate) ? candidate : std::string();
    }

    return std::string();
}

std::string CopperSystem::oxidizedOf(const std::string &identifier) {
    if (!isCopper(identifier) || startsWith(identifier, WAXED) || !startsWith(identifier, PREFIX))
        return std::string();

    const int32_t level = oxidationLevel(identifier);
    if (level >= STAGE_COUNT)
        return std::string();

    std::string rest = identifier.substr(PREFIX.size());
    if (level > 0)
        rest = rest.substr(std::string(OXIDATION_STAGES[level - 1]).size());

    std::string candidate = PREFIX + OXIDATION_STAGES[level] + rest;
    if (!exists(candidate) && rest.size() > 6 && rest.compare(rest.size() - 6, 6, "_block") == 0)
        candidate = PREFIX + OXIDATION_STAGES[level] + rest.substr(0, rest.size() - 6);

    return exists(candidate) ? candidate : std::string();
}

bool CopperSystem::isWeathering(const std::string &identifier) {
    return !oxidizedOf(identifier).empty() || !scrapedOf(identifier).empty();
}

int32_t CopperSystem::oxidationLevel(const std::string &identifier) {
    for (int32_t stage = 0; stage < STAGE_COUNT; stage++) {
        if (startsWith(identifier, PREFIX + OXIDATION_STAGES[stage]))
            return stage + 1;
    }
    return 0;
}

BlockState CopperSystem::transform(const BlockState &source, const std::string &identifier) {
    const Block *block = VanillaBlocks::fromIdentifier(identifier);
    BlockState result = block == nullptr ? BlockState(identifier) : block->toBlockState();

    const std::vector<std::string> keys = result.mStates.getKeys();
    for (const std::string &key: keys) {
        const Tag *value = source.mStates.get(key);
        if (value != nullptr)
            result.mStates.put(key, *value);
    }

    return result;
}

BlockState CopperSystem::replaceWithPair(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &source, const std::string &identifier) {
    const BlockState result = transform(source, identifier);
    level.setBlockState(position.x, position.y, position.z, result);
    BlockActionHandler::broadcastBlockUpdate(owner, level, position, result);

    const BlockActor *blockActor = level.getBlockActors().find(position);
    if (blockActor != nullptr)
        BlockActionHandler::broadcastBlockActorData(owner, level, *blockActor);

    if (!source.mStates.contains(UPPER_BLOCK_BIT))
        return result;

    const int32_t offset = source.mStates.getByte(UPPER_BLOCK_BIT, 0) != 0 ? -1 : 1;
    const Vector3i other(position.x, position.y + offset, position.z);
    const BlockState otherState = level.getBlockState(other.x, other.y, other.z);
    if (otherState.mName != source.mName)
        return result;

    const BlockState otherResult = transform(otherState, identifier);
    level.setBlockState(other.x, other.y, other.z, otherResult);
    BlockActionHandler::broadcastBlockUpdate(owner, level, other, otherResult);
    return result;
}

void CopperSystem::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) {
    if (!isCopper(state.mName) || state.mStates.getByte(UPPER_BLOCK_BIT, 0) != 0)
        return;

    const std::string next = oxidizedOf(state.mName);
    if (next.empty())
        return;

    if (RandomTickSystem::nextInt(OXIDATION_CHANCE_DENOMINATOR) >= OXIDATION_CHANCE_NUMERATOR)
        return;

    const int32_t level0 = oxidationLevel(state.mName);
    int32_t same = 0;
    int32_t further = 0;

    for (int32_t dx = -NEIGHBOUR_RADIUS; dx <= NEIGHBOUR_RADIUS; ++dx) {
        for (int32_t dy = -NEIGHBOUR_RADIUS; dy <= NEIGHBOUR_RADIUS; ++dy) {
            for (int32_t dz = -NEIGHBOUR_RADIUS; dz <= NEIGHBOUR_RADIUS; ++dz) {
                if ((dx == 0 && dy == 0 && dz == 0) || std::abs(dx) + std::abs(dy) + std::abs(dz) > NEIGHBOUR_RADIUS)
                    continue;

                const BlockState *neighbour = level.peekBlockPtr(position.x + dx, position.y + dy, position.z + dz);
                if (neighbour == nullptr || !isWeathering(neighbour->mName))
                    continue;

                const int32_t neighbourLevel = oxidationLevel(neighbour->mName);
                if (neighbourLevel < level0)
                    return;

                if (neighbourLevel > level0)
                    ++further;
                else
                    ++same;
            }
        }
    }

    float chance = (float) (further + 1) / (float) (further + same + 1);
    chance = chance * chance * (level0 == 0 ? UNAFFECTED_MULTIPLIER : 1.0f);
    if (!roll(chance))
        return;

    replaceWithPair(owner, level, position, state, next);
}
