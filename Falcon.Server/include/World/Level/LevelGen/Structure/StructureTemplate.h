#pragma once

#include "Core/NBT/Tag.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// a piece as the game ships it: palette entries keep the file's block names and properties, not the server's
class StructureTemplate {
public:
    struct PaletteEntry {
        std::string mName;
        std::map<std::string, std::string> mProperties;
    };

    struct BlockEntry {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        int32_t mPaletteIndex = 0;
        // block actor data, a compound without entries when the block has none
        Tag mData = Tag::ofCompound();
    };

    // the template stored at a path of the embedded archive such as "igloo/igloo_bottom.nbt", or null
    static std::shared_ptr<const StructureTemplate> load(const std::string &path);

    // quarter turns clockwise seen from above, about the template origin
    static void rotate(int32_t quarterTurns, int32_t &x, int32_t &z);

    // the template settings' mirror, applied before the turn: 1 flips z, 2 flips x, 3 neither
    static void mirror(int32_t mirror, int32_t &x, int32_t &z);

    int32_t getSizeX() const;

    int32_t getSizeY() const;

    int32_t getSizeZ() const;

    const std::vector<PaletteEntry> &getPalette() const;

    const std::vector<BlockEntry> &getBlocks() const;

private:
    bool _read(const Tag &root);

    int32_t mSizeX = 0;
    int32_t mSizeY = 0;
    int32_t mSizeZ = 0;
    std::vector<PaletteEntry> mPalette;
    std::vector<BlockEntry> mBlocks;
};
