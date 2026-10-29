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

    explicit SubChunk(int8_t y);

    int8_t getY() const { return mY; }

    const BlockState &getBlock(int x, int y, int z) const;

    const BlockState &getBlock(int x, int y, int z, int layer) const;

    void setBlock(int x, int y, int z, const BlockState &state);

    void setBlock(int x, int y, int z, int layer, const BlockState &state);

    bool isEmpty() const;

    bool isLayerEmpty(int layer) const;

    bool hasLightEmitter() const;

    bool hasDynamicShapes() const;

    uint32_t getBiome(int x, int y, int z) const;

    void setBiome(int x, int y, int z, uint32_t biomeId);

    // Drops the index array of a section that ended up holding a single biome, which is most of them.
    void compactBiomes();

    void writeBiomes(BinaryStream &stream, bool persistent) const;

    bool readBiomes(ReadOnlyBinaryStream &stream);

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
};
