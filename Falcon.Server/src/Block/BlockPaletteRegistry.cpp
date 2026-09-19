#include "Block/BlockPaletteRegistry.h"

#include "BlockPaletteNbt.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <cstring>
#include <zlib.h>

namespace {
    const int GZIP_WINDOW_BITS = 16 + MAX_WBITS;
    const size_t DECOMPRESS_CHUNK_SIZE = 65536;

    bool gzipDecompress(const std::string &input, std::string &output) {
        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));

        if (inflateInit2(&stream, GZIP_WINDOW_BITS) != Z_OK)
            return false;

        stream.next_in = (Bytef *) input.data();
        stream.avail_in = (uInt) input.size();

        output.clear();
        std::string buffer(DECOMPRESS_CHUNK_SIZE, '\0');

        int result;
        do {
            stream.next_out = (Bytef *) &buffer[0];
            stream.avail_out = (uInt) buffer.size();

            result = inflate(&stream, Z_NO_FLUSH);
            if (result != Z_OK && result != Z_STREAM_END) {
                inflateEnd(&stream);
                return false;
            }

            output.append(buffer.data(), buffer.size() - stream.avail_out);
        } while (result != Z_STREAM_END);

        inflateEnd(&stream);
        return true;
    }
}

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
    if (!gzipDecompress(compressed, decompressed)) {
        LOG_WARN(LogAreaID::Server, "Failed to decompress embedded block palette");
        return;
    }

    ReadOnlyBinaryStream stream(decompressed);

    EncodingSettings trustedSettings;
    trustedSettings.mMaxListSize = 65536;
    stream.setEncodingSettings(trustedSettings);

    Tag root;
    try {
        root = NbtIo::readTag(stream, NbtVariant::BigEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Failed to parse block palette: %s", exception.what());
        return;
    }

    const Tag *blocksTag = root.get("blocks");
    if (blocksTag == nullptr) {
        LOG_WARN(LogAreaID::Server, "Embedded block palette is missing blocks list");
        return;
    }

    for (const Tag &entry: blocksTag->getList()) {
        const Tag *nameTag = entry.get("name");
        const Tag *statesTag = entry.get("states");
        if (nameTag == nullptr || statesTag == nullptr)
            continue;

        const std::string &name = nameTag->asString();
        std::vector<Tag> &permutations = mPermutations[name];
        permutations.push_back(*statesTag);
        if (mDefaultStates.find(name) != mDefaultStates.end())
            continue;

        mBlockNames.push_back(name);
        mDefaultStates.emplace(name, *statesTag);

        const Tag *networkIdTag = entry.get("network_id");
        if (networkIdTag != nullptr)
            mDefaultNetworkId.emplace(name, networkIdTag->asInt());
    }

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
