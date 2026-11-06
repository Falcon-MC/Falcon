#include "World/Level/LevelGen/Structure/StructureMarkers.h"

namespace {
    const uint64_t BUCKET_MASK = 7;

    uint64_t hashCoordinate(int32_t value) {
        uint64_t hash = 0xCBF29CE484222325ULL;
        for (int32_t shift = 0; shift < 32; shift += 8)
            hash = (hash ^ (((uint32_t) value >> shift) & 0xFFU)) * 0x100000001B3ULL;

        return hash + 2654435769ULL;
    }

    uint64_t combine(uint64_t seed, uint64_t hash) {
        return seed ^ ((seed << 6) + (seed >> 2) + hash);
    }

    uint64_t bucketOf(const StructureMarkers::Marker &marker) {
        return combine(combine(hashCoordinate(marker.x), hashCoordinate(marker.y)), hashCoordinate(marker.z))
               & BUCKET_MASK;
    }
}

std::vector<StructureMarkers::Marker> StructureMarkers::inGameOrder(const std::vector<Marker> &markers) {
    std::vector<Marker> ordered;
    for (const Marker &marker : markers) {
        // a new key goes in front of its bucket, and an empty bucket opens at the end of the list
        auto position = ordered.begin();
        while (position != ordered.end() && bucketOf(*position) != bucketOf(marker))
            ++position;

        ordered.insert(position, marker);
    }

    return ordered;
}
