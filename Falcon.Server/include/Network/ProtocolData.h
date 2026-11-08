#pragma once

#include <cstddef>
#include <cstdint>

// game data of an older accepted protocol; generated from the protocols the build embeds
struct ProtocolData {
    int32_t mProtocol;
    const char *mItemPalette;
    const unsigned char *mItemComponents;
    size_t mItemComponentsSize;
    const char *mRecipes;
    const char *mCreativeItems;
    const unsigned char *mBlockPalette;
    size_t mBlockPaletteSize;

    // the oldest embedded protocol not older than it; nullptr when newer than all: the server's own data
    static const ProtocolData *find(int32_t protocol);
};
