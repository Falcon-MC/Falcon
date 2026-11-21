#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>

namespace BannerPatterns {
    // a loom stops adding past this many layers
    constexpr int32_t MAX_LOOM_PATTERNS = 6;

    constexpr int16_t NO_ITEM = -1;

    struct Pattern {
        const char *mName;
        // what the banner's Patterns list saves
        const char *mId;
        // the banner pattern item the loom wants in its material slot, NO_ITEM for dye alone
        int16_t mItemIndex;
        // never offered by the loom: the plain base and the ominous banner's illager
        bool mHidden;
    };

    const Pattern *find(const std::string &id);

    // the pattern index a banner pattern item carries, NO_ITEM for any other item
    int16_t itemIndexOf(const ItemStack &item);

    // the dye's color counted from black, as banner layers save it; 0xFF for anything but a dye
    uint8_t dyeColorOf(const ItemStack &item);

    bool isBanner(const ItemStack &item);

    int32_t patternCount(const ItemStack &banner);

    // the game's loom result: one banner with the pattern laid on top, air when the slots don't allow it
    ItemStack loomResult(const ItemStack &banner, const ItemStack &dye, const ItemStack &material,
                         const Pattern &pattern);
}
