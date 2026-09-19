#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Block/BlockState.h"

#include <cstdint>
#include <vector>

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

    uint32_t getBiome(int x, int y, int z) const;

    void setBiome(int x, int y, int z, uint32_t biomeId);

    void writeBiomes(BinaryStream &stream, bool persistent) const;

    bool readBiomes(ReadOnlyBinaryStream &stream);

    void writeNetwork(BinaryStream &stream) const;

    void writePersistent(BinaryStream &stream) const;

    bool readPersistent(ReadOnlyBinaryStream &stream, bool *replacedUnknown = nullptr);

private:
    static int _index(int x, int y, int z) { return (x << 8) | (z << 4) | y; }

    static int _bitsPerBlock(size_t paletteSize);

    void _writeStorage(BinaryStream &stream, bool persistent,
                       const std::vector<BlockState> &palette,
                       const std::vector<uint16_t> &blocks) const;

    void _writeEmptyStorage(BinaryStream &stream, bool persistent) const;

    bool _readStorage(ReadOnlyBinaryStream &stream, std::vector<BlockState> &palette,
                      std::vector<uint16_t> &blocks, bool *replacedUnknown);

    void _ensureLayer1();

    uint16_t _paletteIndexFor(const BlockState &state);

    uint16_t _paletteIndexForLayer1(const BlockState &state);

    uint16_t _biomePaletteIndexFor(uint32_t biomeId);

    int8_t mY;
    std::vector<BlockState> mPalette;
    std::vector<uint16_t> mBlocks;
    std::vector<BlockState> mPalette2;
    std::vector<uint16_t> mBlocks2;
    std::vector<uint32_t> mBiomePalette;
    std::vector<uint16_t> mBiomes;
};
