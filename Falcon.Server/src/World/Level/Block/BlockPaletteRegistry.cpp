#include "World/Level/Block/BlockPaletteRegistry.h"

#include "BlockPaletteNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <utility>

BlockPaletteRegistry::BlockPaletteRegistry() : mLoaded(false) {
}

BlockPaletteRegistry &BlockPaletteRegistry::getInstance() {
    static BlockPaletteRegistry instance;
    return instance;
}

bool BlockPaletteRegistry::isLoaded() const {
    return mLoaded;
}

void BlockPaletteRegistry::initialize() {
    if (mLoaded)
        return;

    const std::string compressed((const char *) FalconBlockPaletteData::kBlockPaletteNbt,
                                 FalconBlockPaletteData::kBlockPaletteNbtSize);

    std::string decompressed;
    if (!Gzip::decompress(compressed, decompressed)) {
        LOG_WARN(LogAreaID::Server, "Failed to decompress embedded block palette");
        return;
    }

    ReadOnlyBinaryStream stream(decompressed);

    EncodingSettings trustedSettings;
    trustedSettings.mMaxListSize = 65536;
    stream.setEncodingSettings(trustedSettings);

    BlockPaletteRegistry palette;
    bool hasBlocks = false;
    try {
        if (stream.getByte() != (unsigned char) Tag::Type::Compound)
            throw NbtException("Expected a compound block palette");
        NbtIo::readValue(stream, Tag::Type::String, NbtVariant::BigEndian);

        for (;;) {
            const unsigned char typeId = stream.getByte();
            if (!Tag::isValidType(typeId))
                throw NbtException("Invalid block palette tag type");
            const Tag::Type type = (Tag::Type) typeId;
            if (type == Tag::Type::End)
                break;

            const std::string key = NbtIo::readValue(stream, Tag::Type::String, NbtVariant::BigEndian).asString();
            if (key != "blocks" || type != Tag::Type::List) {
                NbtIo::readValue(stream, type, NbtVariant::BigEndian, NbtIo::MAX_DEPTH - 1);
                continue;
            }

            if (stream.getByte() != (unsigned char) Tag::Type::Compound)
                throw NbtException("Expected compound block palette entries");
            const int32_t count = stream.getSignedInt();
            if (count < 0 || (size_t) count > trustedSettings.mMaxListSize)
                throw NbtException("Invalid block palette list size");
            hasBlocks = true;

            // Consume one entry at a time and move its states into the registry, rather than
            // retaining the whole NBT tree and deep-copying every permutation out of it.
            for (int32_t i = 0; i < count; ++i) {
                Tag entry = NbtIo::readValue(stream, Tag::Type::Compound, NbtVariant::BigEndian,
                                             NbtIo::MAX_DEPTH - 2);
                const Tag *nameTag = entry.get("name");
                Tag *statesTag = entry.get("states");
                if (nameTag == nullptr || statesTag == nullptr)
                    continue;

                const std::string &name = nameTag->asString();
                if (palette.mDefaultStates.find(name) == palette.mDefaultStates.end()) {
                    palette.mBlockNames.push_back(name);
                    palette.mDefaultStates.emplace(name, *statesTag);
                    const Tag *networkIdTag = entry.get("network_id");
                    if (networkIdTag != nullptr)
                        palette.mDefaultNetworkId.emplace(name, networkIdTag->asInt());
                }
                palette.mPermutations[name].push_back(std::move(*statesTag));
            }
        }
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Failed to parse block palette: %s", exception.what());
        return;
    }

    if (!hasBlocks) {
        LOG_WARN(LogAreaID::Server, "Embedded block palette is missing blocks list");
        return;
    }

    *this = std::move(palette);
    mLoaded = true;
    LOG_INFO(LogAreaID::Server, "Loaded %zu block palette default states", mDefaultStates.size());
}

const Tag *BlockPaletteRegistry::getDefaultStates(const std::string &identifier) const {
    const std::unordered_map<std::string, Tag>::const_iterator found = mDefaultStates.find(identifier);
    if (found == mDefaultStates.end())
        return nullptr;

    return &found->second;
}

const std::vector<std::string> &BlockPaletteRegistry::getBlockNames() const {
    return mBlockNames;
}

const std::vector<Tag> *BlockPaletteRegistry::getPermutations(const std::string &identifier) const {
    const auto found = mPermutations.find(identifier);
    if (found == mPermutations.end())
        return nullptr;

    return &found->second;
}

bool BlockPaletteRegistry::getDefaultNetworkId(const std::string &identifier, int32_t &out) const {
    const std::unordered_map<std::string, int32_t>::const_iterator found = mDefaultNetworkId.find(identifier);
    if (found == mDefaultNetworkId.end())
        return false;

    out = found->second;
    return true;
}
