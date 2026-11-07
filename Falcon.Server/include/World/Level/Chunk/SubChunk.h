#pragma once

#include "World/Level/Chunk/PaletteIndices.h"
#include "Core/Utility/BinaryStream.h"
#include "World/Level/Block/BlockState.h"
#include "Protocol/Types/BlockNetworkIdMap.h"

#include <cstdint>
#include <vector>

/**
 * A 16x16x16 section stored as palette indices, with a second, lazily allocated layer for
 * waterlogging and per-block biome ids.
 */
class SubChunk {
public:
    static const int BLOCK_COUNT = 4096;
    static const uint8_t STORAGE_VERSION = 9;
    static const uint8_t LAYER_COUNT = 2;
    // one-byte biome section that repeats the section below
    static const uint8_t BIOME_COPY_PREVIOUS = 0xff;

    explicit SubChunk(int8_t y);

    /**
     * The shared copy of a block state, which lives as long as the process. Sections hold these, so a
     * state passed back by address is found in a palette without comparing it.
     */
    static const BlockState *intern(const BlockState &state);

    int8_t getY() const { return mY; }

    const BlockState &getBlock(int x, int y, int z) const;

    const BlockState &getBlock(int x, int y, int z, int layer) const;

    void setBlock(int x, int y, int z, const BlockState &state);

    void setBlock(int x, int y, int z, int layer, const BlockState &state);

    bool isEmpty() const;

    bool isLayerEmpty(int layer) const;

    bool hasLightEmitter() const;

    bool hasDynamicShapes() const;

    // Questions asked of a section's palettes often enough to be remembered; each always comes with the same test.
    enum class PaletteQuery : uint8_t {
        RandomTick = 0,
        Water = 1
    };

    /**
     * Whether a state of either layer's palette passes `matches`, remembered until a palette changes. A palette may
     * keep a state no block uses any more, so a true answer only means the section is worth scanning.
     */
    bool paletteMatches(PaletteQuery query, bool (*matches)(const BlockState &)) const;

    uint32_t getBiome(int x, int y, int z) const;

    void setBiome(int x, int y, int z, uint32_t biomeId);

    // Gives every block of the section the same biome.
    void fillBiome(uint32_t biomeId);

    // Drops the index array of a section that ended up holding a single biome, which is most of them.
    void compactBiomes();

    void writeBiomes(BinaryStream &stream, bool persistent) const;

    // a section saved as a copy of the previous one takes previous's biomes
    bool readBiomes(ReadOnlyBinaryStream &stream, const SubChunk *previous);

    /**
     * Writes the section as a client sees it, with its block states translated by blockIds when it is set.
     */
    void writeNetwork(BinaryStream &stream, const BlockNetworkIdMap *blockIds = nullptr) const;

    void writePersistent(BinaryStream &stream) const;

    bool readPersistent(ReadOnlyBinaryStream &stream, bool *replacedUnknown = nullptr);

private:
    static int _index(int x, int y, int z) { return (x << 8) | (z << 4) | y; }

    static int _bitsPerBlock(size_t paletteSize);

    void _writeStorage(BinaryStream &stream, bool persistent,
                       const std::vector<const BlockState *> &palette,
                       const PaletteIndices &blocks, const BlockNetworkIdMap *blockIds) const;

    void _writeEmptyStorage(BinaryStream &stream, bool persistent, const BlockNetworkIdMap *blockIds) const;

    bool _readStorage(ReadOnlyBinaryStream &stream, std::vector<const BlockState *> &palette,
                      PaletteIndices &blocks, bool *replacedUnknown);

    void _ensureLayer1();

    uint16_t _paletteIndexFor(const BlockState &state);

    uint16_t _paletteIndexForLayer1(const BlockState &state);

    uint16_t _biomePaletteIndexFor(uint32_t biomeId);

    int8_t mY;
    // Entries point at interned states shared by every sub-chunk, so a palette entry costs a pointer.
    std::vector<const BlockState *> mPalette;
    PaletteIndices mBlocks;
    std::vector<const BlockState *> mPalette2;
    PaletteIndices mBlocks2;
    std::vector<uint32_t> mBiomePalette;
    PaletteIndices mBiomes;
    void _forgetPaletteQueries() const {
        mPaletteQueries[0] = -1;
        mPaletteQueries[1] = -1;
        mEmptyCache = -1;
    }

    // per PaletteQuery: -1 until asked, then whether a palette state matched
    mutable int8_t mPaletteQueries[2] = {-1, -1};
    // -1 until isEmpty() runs, then its answer; it only changes with a palette or the second layer's blocks
    mutable int8_t mEmptyCache = -1;
};
