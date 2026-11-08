#include "World/Level/ClientSideGeneration.h"

#include "Core/NBT/Tag.h"

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <algorithm>
#include <cmath>

namespace {
    int64_t packChunk(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }

    int32_t floorDiv(int32_t value, int32_t divisor) {
        int32_t quotient = value / divisor;
        if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
            --quotient;

        return quotient;
    }

    // sub_14381FD00: a ticking area keeps the chunks whose distance to its centre is at most half its width,
    // plus the neighbourhood margin. The width is the chunk count along x only.
    bool isKeptByTickingArea(const ClientSideGeneration::TickingAreaBounds &area, int32_t chunkX, int32_t chunkZ) {
        const int32_t width = area.mMaxChunkX - area.mMinChunkX + 1;
        const int32_t halfWidth = width * 16 / 32;
        const int32_t centreX = (int32_t) ((float) (area.mMinChunkX + area.mMaxChunkX) * 0.5f);
        const int32_t centreZ = (int32_t) ((float) (area.mMinChunkZ + area.mMaxChunkZ) * 0.5f);
        const int64_t dx = centreX - chunkX;
        const int64_t dz = centreZ - chunkZ;
        return dx * dx + dz * dz <= (int64_t) halfWidth * halfWidth + 36;
    }

    // sub_14381FD00: the part of the decision that does not touch storage.
    bool isKeptByPlayersOrAreas(int32_t chunkX, int32_t chunkZ, const ClientSideGeneration::BuildContext &context) {
        // Without any player the game keeps everything on the server.
        if (context.mPlayerCount == 0)
            return true;

        const int64_t reach = context.mBuildRadius + ClientSideGeneration::PLAYER_MARGIN;
        for (const std::pair<int32_t, int32_t> &player: context.mPlayerChunks) {
            const int64_t dx = chunkX - player.first;
            const int64_t dz = chunkZ - player.second;
            if (dx * dx + dz * dz <= reach * reach)
                return true;
        }

        for (const ClientSideGeneration::TickingAreaBounds &area: context.mTickingAreas) {
            if (isKeptByTickingArea(area, chunkX, chunkZ))
                return true;
        }

        return false;
    }
}

float ClientSideGeneration::computeServerBuildRatio(int32_t serverViewDistance, int32_t clientMaxRadius) {
    // sub_14102BCD0 reads the server view distance through a byte getter (sub_141030B00) and the client radius
    // from the byte RequestChunkRadiusPacket carries.
    const uint8_t serverRadius = (uint8_t) serverViewDistance;
    const uint8_t clientRadius = (uint8_t) clientMaxRadius;

    const uint8_t usable = serverRadius >= 6 ? serverRadius : (uint8_t) MIN_BUILD_RADIUS;
    const uint8_t weighed = usable < MAX_WEIGHED_RADIUS ? usable : (uint8_t) MAX_WEIGHED_RADIUS;

    float clientWeight = 1.0f;
    if (clientRadius >= 9) {
        const float distance = std::fabs((float) clientRadius - 28.0f) / 23.0f;
        clientWeight = distance <= 1.0f ? std::max(distance, 0.0f) : 1.0f;
    }

    if (clientWeight <= 0.0f)
        return 0.0f;

    // A client that cannot even handle nine chunks leaves everything to the server.
    if (clientRadius < 9)
        return 1.0f;

    const float serverShare = (float) (int8_t) (weighed - MIN_BUILD_RADIUS) / 23.0f;
    const float serverWeight = 1.0f < serverShare ? 1.0f : std::max(serverShare, 0.0f);
    float ratio = std::min(clientWeight, serverWeight);
    if (clientRadius >= weighed)
        return ratio;

    const float larger = std::max(clientWeight, serverWeight);
    const uint32_t gap = (uint32_t) (weighed - clientRadius);
    if (gap > 16)
        return larger;

    if (gap >= 11) {
        if (serverRadius < 18)
            return ratio;

        return clientRadius >= 16 ? ratio : larger;
    }

    if (serverRadius <= 17 && gap >= 5)
        ratio = larger - ratio;

    return ratio;
}

int32_t ClientSideGeneration::computeServerBuildRadius(float ratio, int32_t viewRadius, int32_t tickRange) {
    if (ratio <= 0.0f)
        return tickRange >= 6 ? tickRange : MIN_BUILD_RADIUS;

    const int32_t scaled = (int32_t) (int64_t) (ratio * (float) viewRadius);
    int32_t radius = scaled >= 6 ? scaled : MIN_BUILD_RADIUS;
    if (viewRadius < scaled)
        radius = viewRadius;

    return radius;
}

int32_t ClientSideGeneration::computeViewRadius(int32_t chunkRadius, int32_t tickRange) {
    return std::max(chunkRadius, tickRange) + PLAYER_MARGIN;
}

bool ClientSideGeneration::isNeighbourOffset(int32_t dx, int32_t dz) {
    if (dx < -NEIGHBOUR_RADIUS || dx > NEIGHBOUR_RADIUS || dz < -NEIGHBOUR_RADIUS || dz > NEIGHBOUR_RADIUS)
        return false;

    return (float) dx * (float) dx + (float) dz * (float) dz < NEIGHBOUR_REACH_SQUARED;
}

bool ClientSideGeneration::isServerBuilt(int32_t chunkX, int32_t chunkZ, const BuildContext &context) {
    if (context.mIsForced && context.mIsForced(chunkX, chunkZ))
        return true;

    // Chunks needing blending with pre-1.18 terrain (ChunkSource slot 38) never apply: Falcon does not blend.
    if (context.mIsSaved && context.mIsSaved(chunkX, chunkZ))
        return true;

    if (isKeptByPlayersOrAreas(chunkX, chunkZ, context))
        return true;

    if (!context.mIsSaved)
        return false;

    // A saved chunk nearby keeps this one on the server too, so the client never builds next to terrain the
    // server holds.
    for (int32_t dz = -NEIGHBOUR_RADIUS; dz <= NEIGHBOUR_RADIUS; ++dz) {
        for (int32_t dx = -NEIGHBOUR_RADIUS; dx <= NEIGHBOUR_RADIUS; ++dx) {
            if ((dx == 0 && dz == 0) || !isNeighbourOffset(dx, dz))
                continue;

            if (context.mIsSaved(chunkX + dx, chunkZ + dz))
                return true;
        }
    }

    return false;
}

void ClientSideGeneration::SavedChunkList::add(int32_t chunkX, int32_t chunkZ) {
    if (!mKeys.insert(packChunk(chunkX, chunkZ)).second)
        return;

    mChunks.push_back(Vector2i(chunkX, chunkZ));
}

std::vector<Vector2i> ClientSideGeneration::SavedChunkList::drain() {
    std::vector<Vector2i> chunks;
    chunks.swap(mChunks);
    mKeys.clear();
    return chunks;
}

void ClientSideGeneration::SavedChunkList::clear() {
    mChunks.clear();
    mKeys.clear();
}

bool ClientSideGeneration::applyClientMapPixels(Tag &mapData, const std::vector<MapPixel> &pixels) {
    if (!mapData.isCompound() || pixels.empty())
        return false;

    std::vector<int8_t> colors;
    if (const Tag *stored = mapData.get("colors")) {
        if (stored->getType() == Tag::Type::ByteArray)
            colors = stored->asByteArray();
    }
    colors.resize((size_t) MAP_PIXEL_COUNT * 4, 0);

    bool changed = false;
    for (const MapPixel &pixel: pixels) {
        if (pixel.mPixel == 0 || pixel.mIndex < 0 || pixel.mIndex >= MAP_PIXEL_COUNT)
            continue;

        // The game keeps each pixel as a 32-bit colour and saves the array as it lies in memory.
        const uint32_t colour = (uint32_t) pixel.mPixel;
        int8_t *bytes = &colors[(size_t) pixel.mIndex * 4];
        uint32_t current = (uint32_t) (uint8_t) bytes[0] | ((uint32_t) (uint8_t) bytes[1] << 8)
                           | ((uint32_t) (uint8_t) bytes[2] << 16) | ((uint32_t) (uint8_t) bytes[3] << 24);
        if (current == colour)
            continue;

        bytes[0] = (int8_t) (colour & 0xff);
        bytes[1] = (int8_t) ((colour >> 8) & 0xff);
        bytes[2] = (int8_t) ((colour >> 16) & 0xff);
        bytes[3] = (int8_t) ((colour >> 24) & 0xff);
        changed = true;
    }

    if (changed)
        mapData.put("colors", Tag::ofByteArray(std::move(colors)));
    return changed;
}

const ClientSideGeneration::StructureGuard::Start &
ClientSideGeneration::StructureGuard::_start(const ChunkGenerator &generator, const StructureFeature &feature,
                                             int32_t regionX, int32_t regionZ) {
    std::unordered_map<int64_t, Start> &starts = mStarts[&feature];
    const int64_t key = packChunk(regionX, regionZ);
    const auto known = starts.find(key);
    if (known != starts.end())
        return known->second;

    const StructureChunkPos candidate = feature.getCandidate(generator.getSeed(), regionX, regionZ);
    Start start;
    start.mX = candidate.x;
    start.mZ = candidate.z;
    start.mValid = feature.isStartChunk(generator, candidate.x, candidate.z);
    return starts.emplace(key, start).first->second;
}

bool ClientSideGeneration::StructureGuard::isNearStart(const ChunkGenerator &generator,
                                                       const std::vector<const StructureFeature *> &features,
                                                       int32_t chunkX, int32_t chunkZ) {
    for (const StructureFeature *feature: features) {
        const int32_t spacing = feature->getSpacing();
        const int32_t minRegionX = floorDiv(chunkX - STRUCTURE_GUARD_RADIUS, spacing);
        const int32_t maxRegionX = floorDiv(chunkX + STRUCTURE_GUARD_RADIUS, spacing);
        const int32_t minRegionZ = floorDiv(chunkZ - STRUCTURE_GUARD_RADIUS, spacing);
        const int32_t maxRegionZ = floorDiv(chunkZ + STRUCTURE_GUARD_RADIUS, spacing);

        for (int32_t regionX = minRegionX; regionX <= maxRegionX; ++regionX) {
            for (int32_t regionZ = minRegionZ; regionZ <= maxRegionZ; ++regionZ) {
                const Start &start = _start(generator, *feature, regionX, regionZ);
                if (!start.mValid)
                    continue;

                if (std::abs(start.mX - chunkX) <= STRUCTURE_GUARD_RADIUS
                    && std::abs(start.mZ - chunkZ) <= STRUCTURE_GUARD_RADIUS)
                    return true;
            }
        }
    }

    return false;
}
