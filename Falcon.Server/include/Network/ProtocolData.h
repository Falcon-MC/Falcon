#pragma once

#include <cstddef>
#include <cstdint>

/**
 * The game data of an older protocol the server still accepts. The list is generated from the protocols the build
 * embeds data for.
 */
struct ProtocolData {
    int32_t mProtocol;
    const char *mItemPalette;
    const unsigned char *mItemComponents;
    size_t mItemComponentsSize;
    const char *mRecipes;
    const char *mCreativeItems;
    const unsigned char *mBlockPalette;
    size_t mBlockPaletteSize;

    /**
     * The data a client of this protocol uses: that of the oldest embedded protocol not older than it, or nullptr
     * when it is newer than all of them and so uses the server's own data.
     */
    static const ProtocolData *find(int32_t protocol);
};
