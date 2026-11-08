#pragma once

#include <cstdint>
#include <string>
#include <vector>

// the game keeps data blocks in a hash map keyed on world position and draws its random while walking it: the
// order decides what each marker gets
class StructureMarkers {
public:
    struct Marker {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        std::string mText;
    };

    // the markers in the game's walk order, from the file's order; at most eight: up to that size the map keeps
    // its first buckets
    static std::vector<Marker> inGameOrder(const std::vector<Marker> &markers);
};
