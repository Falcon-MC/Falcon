#include "World/Level/Block/LightningRodBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/CopperSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cstdint>
#include <vector>

FALCON_REGISTER_BLOCK(LightningRodBlock, 100);

namespace {
    const char *STATE_POWERED = "powered_bit";

    bool isPowered(const BlockState &state) {
        return state.mStates.getByte(STATE_POWERED, 0) != 0;
    }

    BlockState withPower(const BlockState &state, bool powered) {
        Tag states = state.mStates;
        states.putByte(STATE_POWERED, powered ? 1 : 0);
        return BlockState(state.mName, states);
    }

    // the face the rod points to; its support sits on the opposite side
    int facingOf(const BlockState &state) {
        return std::clamp((int) state.mStates.getInt("facing_direction", RedstoneFace::UP), 0, RedstoneFace::COUNT - 1);
    }

    void updateNeighbours(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                          const BlockState &state) {
        RedstoneSystem::updateAroundRedstone(owner, level, position);
        RedstoneSystem::updateAroundRedstone(owner, level,
                                             RedstoneFace::relative(position, RedstoneFace::opposite(facingOf(state))));
    }
}

bool LightningRodBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(
            identifier, {BlockNames::LIGHTNING_ROD, BlockNames::EXPOSED_LIGHTNING_ROD,
                         BlockNames::WEATHERED_LIGHTNING_ROD, BlockNames::OXIDIZED_LIGHTNING_ROD,
                         BlockNames::WAXED_LIGHTNING_ROD, BlockNames::WAXED_EXPOSED_LIGHTNING_ROD,
                         BlockNames::WAXED_WEATHERED_LIGHTNING_ROD, BlockNames::WAXED_OXIDIZED_LIGHTNING_ROD});
}

// the producer is attached to the support, opposite facing_direction, so only that block is charged
int LightningRodBlock::getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                                        bool strong) const {
    (void) level;
    (void) position;
    if (!isPowered(state))
        return 0;

    return !strong || face == facingOf(state) ? RedstoneSystem::MAX_SIGNAL : 0;
}

// LightningRodBlock tick: the strike's power ends
void LightningRodBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state) const {
    if (!isPowered(state))
        return;

    const BlockState unpowered = withPower(state, false);
    level.setBlock(position, unpowered, true);
    updateNeighbours(owner, level, position, unpowered);
}

// LightningRodBlock::onLightningHit; the weathering rods first lose one oxidation stage
void LightningRodBlock::onLightningHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    BlockState state = level.getBlockState(position.x, position.y, position.z);
    if (!matches(state.mName))
        return;

    const std::string scraped = CopperSystem::scrapedOf(state.mName);
    if (!scraped.empty())
        state = CopperSystem::transform(state, scraped);

    level.cancelUpdate(position);
    const BlockState powered = withPower(state, true);
    level.setBlock(position, powered, true);
    updateNeighbours(owner, level, position, powered);
    level.scheduleUpdate(position, POWERED_TICKS);
    owner.broadcastLevelEvent(level, HIT_EVENT, Vector3f((float) position.x, (float) position.y, (float) position.z),
                              0);
}

// lightning target search: rods in a cylinder of 64 blocks around and 64 up or down,
// nearest first; the first with nothing solid above where it rains takes the bolt
std::optional<Vector3i> LightningRodBlock::findAttractingRod(Level &level, const Vector3i &target) {
    struct Candidate {
        Vector3i mPosition;
        int64_t mDistance;
    };

    std::vector<Candidate> candidates;
    const int64_t radiusSquared = (int64_t) ATTRACT_RADIUS * ATTRACT_RADIUS;
    const int32_t minY = std::max(target.y - ATTRACT_RADIUS, level.getMinY());
    const int32_t maxY = std::min(target.y + ATTRACT_RADIUS, level.getMaxY());

    for (int32_t chunkX = (target.x - ATTRACT_RADIUS) >> 4; chunkX <= (target.x + ATTRACT_RADIUS) >> 4; ++chunkX) {
        for (int32_t chunkZ = (target.z - ATTRACT_RADIUS) >> 4; chunkZ <= (target.z + ATTRACT_RADIUS) >> 4; ++chunkZ) {
            if (!level.isChunkResident(chunkX, chunkZ))
                continue;

            for (int32_t x = chunkX << 4; x < (chunkX << 4) + 16; ++x) {
                for (int32_t z = chunkZ << 4; z < (chunkZ << 4) + 16; ++z) {
                    const int64_t dx = x - target.x;
                    const int64_t dz = z - target.z;
                    const int64_t distance = dx * dx + dz * dz;
                    if (distance > radiusSquared)
                        continue;

                    for (int32_t y = minY; y <= maxY; ++y) {
                        const BlockState *state = level.peekBlockPtr(x, y, z);
                        if (state != nullptr && BlockIdentifier::endsWith(state->mName, "lightning_rod")
                            && matches(state->mName))
                            candidates.push_back({Vector3i(x, y, z), distance});
                    }
                }
            }
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate &a, const Candidate &b) { return a.mDistance < b.mDistance; });

    for (const Candidate &candidate: candidates) {
        const Vector3i &rod = candidate.mPosition;
        bool open = true;
        for (int32_t y = rod.y + 1; y <= level.getMaxY() && open; ++y)
            open = !level.isSolidAt(rod.x, y, rod.z);

        if (open && level.canRainAt(rod.x, rod.z))
            return rod;
    }

    return std::nullopt;
}
