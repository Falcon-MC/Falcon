#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * The data blocks of a structure piece. The game keeps them in a hash map keyed on their world position and draws
 * its random while walking that map, so their order decides what each marker gets.
 */
class StructureMarkers {
public:
    struct Marker {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        std::string mText;
    };

    // the markers of a piece as the game walks them, from the order of the file; at most eight, the size up to which
    // the map keeps its first buckets
    static std::vector<Marker> inGameOrder(const std::vector<Marker> &markers);
};
