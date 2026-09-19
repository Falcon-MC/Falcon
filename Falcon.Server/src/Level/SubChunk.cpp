#include "Level/SubChunk.h"

#include "Block/BlockLightProperties.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Core/BlockState/BlockStateUpgradeSchema.h"
#include "Core/Debug/BedrockLog.h"
#include "Level/BlockStateUpgrades.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Core/NBT/NbtIo.h"

SubChunk::SubChunk(int8_t y) : mY(y) {
    mPalette.push_back(BlockState("minecraft:air"));
    mBlocks.assign(BLOCK_COUNT, 0);
    mBiomePalette.push_back(1);
}

uint16_t SubChunk::_biomePaletteIndexFor(uint32_t biomeId) {
    for (size_t i = 0; i < mBiomePalette.size(); i++) {
        if (mBiomePalette[i] == biomeId)
            return (uint16_t) i;
    }

    mBiomePalette.push_back(biomeId);
    return (uint16_t) (mBiomePalette.size() - 1);
}

uint32_t SubChunk::getBiome(int x, int y, int z) const {
    if (mBiomes.empty())
        return mBiomePalette[0];

    return mBiomePalette[mBiomes[_index(x, y, z)]];
}

void SubChunk::setBiome(int x, int y, int z, uint32_t biomeId) {
    const uint16_t index = _biomePaletteIndexFor(biomeId);

    if (mBiomes.empty()) {
        if (index == 0)
            return;

        mBiomes.assign(BLOCK_COUNT, 0);
    }

    mBiomes[_index(x, y, z)] = index;
}

void SubChunk::writeBiomes(BinaryStream &stream, bool persistent) const {
    if (mBiomePalette.size() <= 1) {
        stream.putByte((unsigned char) ((0 << 1) | (persistent ? 0 : 1)));

        if (persistent)
            stream.putLInt((uint32_t) mBiomePalette[0]);
        else
            stream.putVarInt((int32_t) mBiomePalette[0]);

        return;
    }

    const int bitsPerBlock = _bitsPerBlock(mBiomePalette.size());
    const int blocksPerWord = 32 / bitsPerBlock;
    const int wordCount = (BLOCK_COUNT + blocksPerWord - 1) / blocksPerWord;

    stream.putByte((unsigned char) ((bitsPerBlock << 1) | (persistent ? 0 : 1)));

    int blockIndex = 0;
    for (int word = 0; word < wordCount; word++) {
        uint32_t packed = 0;
        for (int slot = 0; slot < blocksPerWord && blockIndex < BLOCK_COUNT; slot++) {
            packed |= ((uint32_t) mBiomes[blockIndex]) << (slot * bitsPerBlock);
            blockIndex++;
        }
        stream.putLInt(packed);
    }

    if (persistent) {
        stream.putLInt((uint32_t) mBiomePalette.size());
        for (uint32_t biomeId: mBiomePalette)
            stream.putLInt(biomeId);
        return;
    }

    stream.putVarInt((int32_t) mBiomePalette.size());
    for (uint32_t biomeId: mBiomePalette)
        stream.putVarInt((int32_t) biomeId);
}

bool SubChunk::readBiomes(ReadOnlyBinaryStream &stream) {
    const unsigned char header = stream.getByte();
    const int bitsPerBlock = header >> 1;

    if (bitsPerBlock == 0) {
        const uint32_t biomeId = stream.getLInt();
        mBiomePalette.assign(1, biomeId);
        mBiomes.clear();
        return true;
    }

    if (bitsPerBlock > 16)
        return false;

    const int blocksPerWord = 32 / bitsPerBlock;
    const int wordCount = (BLOCK_COUNT + blocksPerWord - 1) / blocksPerWord;
    const uint32_t mask = (1u << bitsPerBlock) - 1u;

    mBiomes.assign(BLOCK_COUNT, 0);

    int blockIndex = 0;
    for (int word = 0; word < wordCount; word++) {
        const uint32_t packed = stream.getLInt();
        for (int slot = 0; slot < blocksPerWord && blockIndex < BLOCK_COUNT; slot++) {
            mBiomes[blockIndex] = (uint16_t) ((packed >> (slot * bitsPerBlock)) & mask);
            blockIndex++;
        }
    }

    const uint32_t paletteSize = stream.getLInt();
    if (paletteSize == 0 || paletteSize > BLOCK_COUNT)
        return false;

    mBiomePalette.clear();
    mBiomePalette.reserve(paletteSize);

    for (uint32_t i = 0; i < paletteSize; i++)
        mBiomePalette.push_back(stream.getLInt());

    for (uint16_t &index: mBiomes) {
        if (index >= mBiomePalette.size())
            index = 0;
    }

    return true;
}

int SubChunk::_bitsPerBlock(size_t paletteSize) {
    if (paletteSize <= 2)
        return 1;
    if (paletteSize <= 4)
        return 2;
    if (paletteSize <= 8)
        return 3;
    if (paletteSize <= 16)
        return 4;
    if (paletteSize <= 32)
        return 5;
    if (paletteSize <= 64)
        return 6;
    if (paletteSize <= 256)
        return 8;

    return 16;
}

const BlockState &SubChunk::getBlock(int x, int y, int z) const {
    return mPalette[mBlocks[_index(x, y, z)]];
}

const BlockState &SubChunk::getBlock(int x, int y, int z, int layer) const {
    static const BlockState air;

    if (layer <= 0)
        return getBlock(x, y, z);

    if (mBlocks2.empty())
        return air;

    return mPalette2[mBlocks2[_index(x, y, z)]];
}

uint16_t SubChunk::_paletteIndexFor(const BlockState &state) {
    for (size_t i = 0; i < mPalette.size(); i++) {
        if (mPalette[i] == state)
            return (uint16_t) i;
    }

    mPalette.push_back(state);
    return (uint16_t) (mPalette.size() - 1);
}

uint16_t SubChunk::_paletteIndexForLayer1(const BlockState &state) {
    for (size_t i = 0; i < mPalette2.size(); i++) {
        if (mPalette2[i] == state)
            return (uint16_t) i;
    }

    mPalette2.push_back(state);
    return (uint16_t) (mPalette2.size() - 1);
}

void SubChunk::setBlock(int x, int y, int z, const BlockState &state) {
    mBlocks[_index(x, y, z)] = _paletteIndexFor(state);
}

void SubChunk::_ensureLayer1() {
    if (!mBlocks2.empty())
        return;

    mPalette2.push_back(BlockState("minecraft:air"));
    mBlocks2.assign(BLOCK_COUNT, 0);
}

void SubChunk::setBlock(int x, int y, int z, int layer, const BlockState &state) {
    if (layer <= 0) {
        setBlock(x, y, z, state);
        return;
    }

    _ensureLayer1();
    mBlocks2[_index(x, y, z)] = _paletteIndexForLayer1(state);
}

bool SubChunk::isEmpty() const {
    if (mPalette.size() != 1 || mPalette[0].mName != "minecraft:air")
        return false;

    return isLayerEmpty(1);
}

bool SubChunk::hasLightEmitter() const {
    for (const BlockState &state: mPalette) {
        if (BlockLightProperties::lightLevel(BlockLightProperties::packed(state)) > 0)
            return true;
    }

    return false;
}

bool SubChunk::isLayerEmpty(int layer) const {
    if (layer <= 0) {
        if (mPalette.size() != 1)
            return false;

        return mPalette[0].mName == "minecraft:air";
    }

    if (mBlocks2.empty())
        return true;

    for (uint16_t index: mBlocks2) {
        if (mPalette2[index].mName != "minecraft:air")
            return false;
    }

    return true;
}

void SubChunk::_writeStorage(BinaryStream &stream, bool persistent,
                            const std::vector<BlockState> &palette,
                            const std::vector<uint16_t> &blocks) const {
    const int bitsPerBlock = _bitsPerBlock(palette.size());
    const int blocksPerWord = 32 / bitsPerBlock;
    const int wordCount = (BLOCK_COUNT + blocksPerWord - 1) / blocksPerWord;

    stream.putByte((unsigned char) ((bitsPerBlock << 1) | (persistent ? 0 : 1)));

    int blockIndex = 0;
    for (int word = 0; word < wordCount; word++) {
        uint32_t packed = 0;
        for (int slot = 0; slot < blocksPerWord && blockIndex < BLOCK_COUNT; slot++) {
            packed |= ((uint32_t) blocks[blockIndex]) << (slot * bitsPerBlock);
            blockIndex++;
        }
        stream.putLInt(packed);
    }

    if (persistent) {
        stream.putLInt((uint32_t) palette.size());
        for (const BlockState &state: palette)
            NbtIo::writeTag(stream, state.toNbt(), NbtVariant::LittleEndian);
        return;
    }

    stream.putVarInt((int32_t) palette.size());
    for (const BlockState &state: palette)
        stream.putVarInt(state.getHash());
}

void SubChunk::writeNetwork(BinaryStream &stream) const {
    stream.putByte(STORAGE_VERSION);
    stream.putByte(LAYER_COUNT);
    stream.putByte((unsigned char) (signed char) mY);

    _writeStorage(stream, false, mPalette, mBlocks);

    if (isLayerEmpty(1))
        _writeEmptyStorage(stream, false);
    else
        _writeStorage(stream, false, mPalette2, mBlocks2);
}

void SubChunk::_writeEmptyStorage(BinaryStream &stream, bool persistent) const {
    stream.putByte((unsigned char) ((1 << 1) | (persistent ? 0 : 1)));

    for (int word = 0; word < 128; word++)
        stream.putLInt(0);

    const BlockState air;

    if (persistent) {
        stream.putLInt(1);
        NbtIo::writeTag(stream, air.toNbt(), NbtVariant::LittleEndian);
        return;
    }

    stream.putVarInt(1);
    stream.putVarInt(air.getHash());
}

void SubChunk::writePersistent(BinaryStream &stream) const {
    stream.putByte(STORAGE_VERSION);
    stream.putByte(LAYER_COUNT);
    stream.putByte((unsigned char) (signed char) mY);

    _writeStorage(stream, true, mPalette, mBlocks);

    if (isLayerEmpty(1))
        _writeEmptyStorage(stream, true);
    else
        _writeStorage(stream, true, mPalette2, mBlocks2);
}

bool SubChunk::_readStorage(ReadOnlyBinaryStream &stream, std::vector<BlockState> &palette,
                           std::vector<uint16_t> &blocks, bool *replacedUnknown) {
    const unsigned char header = stream.getByte();
    const int bitsPerBlock = header >> 1;
    if (bitsPerBlock <= 0 || bitsPerBlock > 16)
        return false;

    const int blocksPerWord = 32 / bitsPerBlock;
    const int wordCount = (BLOCK_COUNT + blocksPerWord - 1) / blocksPerWord;
    const uint32_t mask = bitsPerBlock == 32 ? 0xffffffffu : ((1u << bitsPerBlock) - 1u);

    blocks.assign(BLOCK_COUNT, 0);

    int blockIndex = 0;
    for (int word = 0; word < wordCount; word++) {
        const uint32_t packed = stream.getLInt();
        for (int slot = 0; slot < blocksPerWord && blockIndex < BLOCK_COUNT; slot++) {
            blocks[blockIndex] = (uint16_t) ((packed >> (slot * bitsPerBlock)) & mask);
            blockIndex++;
        }
    }

    const uint32_t paletteSize = stream.getLInt();
    if (paletteSize == 0 || paletteSize > BLOCK_COUNT)
        return false;

    palette.clear();
    palette.reserve(paletteSize);

    for (uint32_t i = 0; i < paletteSize; i++) {
        const Tag tag = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        const Tag *storedStates = tag.get("states");
        BlockStateData stored(tag.getString("name", "minecraft:air"),
                              storedStates != nullptr && storedStates->isCompound() ? *storedStates
                                                                                    : Tag::ofCompound(),
                              tag.getInt("version"));

        try {
            stored = BlockStateUpgrades::upgrade(stored);
        } catch (const BlockStateUpgradeException &exception) {
            LOG_WARN(LogAreaID::Level, "Could not upgrade block %s: %s", stored.getName().c_str(), exception.what());
        }

        const std::string &name = stored.getName();
        const Tag *states = &stored.getStates();

        if (VanillaBlocks::fromIdentifier(name) == nullptr &&
            !CustomContentRegistry::getInstance().isCustomBlock(name)) {
            LOG_WARN(LogAreaID::Level, "Replaced unknown block %s with air while loading chunk data", name.c_str());
            palette.push_back(BlockState("minecraft:air"));
            if (replacedUnknown != nullptr)
                *replacedUnknown = true;
            continue;
        }

        palette.push_back(states == nullptr ? BlockState(name) : BlockState(name, *states));
    }

    for (uint16_t &index: blocks) {
        if (index >= palette.size())
            index = 0;
    }

    return true;
}

bool SubChunk::readPersistent(ReadOnlyBinaryStream &stream, bool *replacedUnknown) {
    const unsigned char version = stream.getByte();
    const unsigned char layers = stream.getByte();

    if (version >= 9)
        mY = (int8_t) stream.getByte();

    if (layers == 0)
        return false;

    if (!_readStorage(stream, mPalette, mBlocks, replacedUnknown))
        return false;

    mPalette2.clear();
    mBlocks2.clear();

    if (layers >= 2) {
        std::vector<BlockState> palette;
        std::vector<uint16_t> blocks;
        if (!_readStorage(stream, palette, blocks, replacedUnknown))
            return false;

        bool hasContent = false;
        for (uint16_t index: blocks) {
            if (palette[index].mName != "minecraft:air") {
                hasContent = true;
                break;
            }
        }

        if (hasContent) {
            mPalette2 = std::move(palette);
            mBlocks2 = std::move(blocks);
        }
    }

    return true;
}
