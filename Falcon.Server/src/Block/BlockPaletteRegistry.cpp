#include "Block/BlockPaletteRegistry.h"

#include "BlockPaletteNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

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
