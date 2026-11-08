#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>

class ChunkGenerator;
class Level;

// /locate biome; findNearest reads only the biome source and noise terrain (any thread), findOpening reads
// loaded chunks (main thread)
class BiomeSearch {
public:
    struct Result {
        bool mFound = false;
        // the fine pass found it under the surface: the game then looks for open air around it
        bool mNeedsOpening = false;
        Vector3i mPosition;
    };

    // sub_14BE9F0B0
    static Result findNearest(const ChunkGenerator &generator, const Vector3i &origin, int32_t biomeId);

    // sub_14A229EC0
    static Vector3i findOpening(Level &level, const Vector3i &position, int32_t biomeId);

    // loads the chunks findOpening reads around the position, main thread only
    static void loadOpeningArea(Level &level, const Vector3i &position);

private:
    // sub_14A228EB0
    static bool _search(const ChunkGenerator &generator, int32_t biomeId, const Vector3i &center, Vector3i min,
                        Vector3i max, int32_t step, Vector3i &found);

    static bool _searchGroup(const ChunkGenerator &generator, int32_t biomeId, const Vector3i &center,
                             const Vector3i &min, const Vector3i &max, int32_t step, Vector3i &found);

    static bool _isBiomeAt(Level &level, int32_t x, int32_t y, int32_t z, int32_t biomeId);

    static bool _isAirAt(Level &level, int32_t x, int32_t y, int32_t z);
};
