#include "World/Level/Block/SpongeBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <deque>
#include <unordered_set>
#include <utility>

FALCON_REGISTER_BLOCK(SpongeBlock, 357);

namespace {
    // LevelEvent 3501: the steam of a wet sponge drying
    constexpr int32_t SPONGE_DRY = 3501;
    constexpr int ABSORB_LIMIT = 64;
    constexpr int ABSORB_DISTANCE = 6;

    const BlockState &drySponge() {
        static const BlockState state("minecraft:sponge");
        return state;
    }

    const BlockState &wetSponge() {
        static const BlockState state("minecraft:wet_sponge");
        return state;
    }
}

bool SpongeBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:sponge" || identifier == "minecraft:wet_sponge";
}

bool SpongeBlock::_isWet(const BlockState &state) {
    return state.mName == wetSponge().mName;
}

// sub_148F994D0: a wet sponge dries in an ultrawarm dimension, a dry one absorbs at once anywhere else
void SpongeBlock::onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
    Level &level = ticks.getLevel();
    if (level.getDimensionType() != DimensionType::Nether) {
        _tryAbsorb(ticks, position, state);
        return;
    }
    if (!_isWet(state))
        return;

    ticks.write(position, 0, drySponge());
    if (ServerNetworkHandler *owner = level.getOwner())
        owner->broadcastLevelEvent(
                level, SPONGE_DRY,
                Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f), 0);
}

// sub_148F9A1C0: a dry sponge absorbs on a tick one later
void SpongeBlock::onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                         const Vector3i &changed) const {
    (void) changed;
    if (!_isWet(state))
        ticks.queueTick(position, this, 1);
}

// sub_148F99AD0
void SpongeBlock::onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
    _tryAbsorb(ticks, position, state);
}

// sub_148F99FB0: a sponge that took water turns wet and ticks once more
void SpongeBlock::_tryAbsorb(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
    if (_isWet(state) || !_absorb(ticks, position))
        return;
    ticks.write(position, 0, wetSponge());
    ticks.queueTick(position, VanillaBlocks::fromState(ticks.stateAt(position)), 1);
}

// sub_148F9A380: breadth first from the sponge, six shuffled faces a step, 64 blocks at most
bool SpongeBlock::_absorb(LiquidPhysicsSystem &ticks, const Vector3i &origin) {
    static const BlockState air;
    Level &level = ticks.getLevel();
    std::deque<std::pair<Vector3i, int>> queue;
    std::unordered_set<int64_t> visited;
    queue.emplace_back(origin, 0);
    int absorbed = 0;
    while (!queue.empty()) {
        const std::pair<Vector3i, int> current = queue.front();
        queue.pop_front();
        const Vector3i &at = current.first;
        const int64_t key = ((int64_t) (at.x & 0x3ffffff) << 38) | ((int64_t) (at.z & 0x3ffffff) << 12)
                            | (int64_t) (at.y & 0xfff);
        if (!visited.insert(key).second)
            continue;

        // Facing order down, up, north, south, west, east, then swaps drawn from the level random (sub_143532490)
        Vector3i faces[6] = {Vector3i(at.x, at.y - 1, at.z), Vector3i(at.x, at.y + 1, at.z),
                             Vector3i(at.x, at.y, at.z - 1), Vector3i(at.x, at.y, at.z + 1),
                             Vector3i(at.x - 1, at.y, at.z), Vector3i(at.x + 1, at.y, at.z)};
        for (int face = 1; face < 6; ++face) {
            const int other = RandomTickSystem::nextInt(face + 1);
            if (other != face)
                std::swap(faces[face], faces[other]);
        }

        for (const Vector3i &next: faces) {
            if (next.y < level.getMinY() || next.y > level.getMaxY() || !ticks.isLoaded(next))
                continue;

            // a column is not of water material: its water goes with it
            const LiquidProperties block = ticks.propertiesAt(next);
            if (block.material == LiquidMaterial::Water) {
                ticks.write(next, 0, air);
            } else if (block.bubbleColumn) {
                ticks.write(next, 1, air);
                ticks.write(next, 0, air);
            } else if (ticks.propertiesAt(next, 1).material == LiquidMaterial::Water) {
                ticks.write(next, 1, air);
            } else {
                continue;
            }

            if (++absorbed >= ABSORB_LIMIT)
                return true;
            if (current.second < ABSORB_DISTANCE)
                queue.emplace_back(next, current.second + 1);
        }
    }
    return absorbed != 0;
}
