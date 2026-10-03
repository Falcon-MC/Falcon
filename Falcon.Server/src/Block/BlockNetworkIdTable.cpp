#include "Block/BlockNetworkIdTable.h"

#include "Block/BlockPaletteRegistry.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Item/ItemNetworkIdTable.h"
#include "Network/ProtocolData.h"
#include "Protocol/BlockStateHasher.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    const char *AIR = "minecraft:air";

    struct OlderPalette {
        std::unordered_set<int32_t> mHashes;
        std::unordered_map<std::string, std::vector<std::string>> mStateNames;
        std::unordered_map<std::string, int32_t> mDefaultHashes;
    };

    OlderPalette loadPalette(const unsigned char *data, size_t size) {
        OlderPalette palette;

        std::string decompressed;
        if (!Gzip::decompress(std::string((const char *) data, size), decompressed)) {
            LOG_WARN(LogAreaID::Server, "Failed to decompress an embedded block palette");
            return palette;
        }

        ReadOnlyBinaryStream stream(decompressed);
        EncodingSettings trustedSettings;
        trustedSettings.mMaxListSize = 65536;
        stream.setEncodingSettings(trustedSettings);

        Tag root;
        try {
            root = NbtIo::readTag(stream, NbtVariant::BigEndian);
        } catch (const std::exception &exception) {
            LOG_WARN(LogAreaID::Server, "Failed to parse an embedded block palette: %s", exception.what());
            return palette;
        }

        const Tag *blocks = root.get("blocks");
        if (blocks == nullptr)
            return palette;

        for (const Tag &entry: blocks->getList()) {
            const Tag *name = entry.get("name");
            const Tag *states = entry.get("states");
            const Tag *networkId = entry.get("network_id");
            if (name == nullptr || states == nullptr || networkId == nullptr)
                continue;

            palette.mHashes.insert(networkId->asInt());
            if (palette.mDefaultHashes.emplace(name->asString(), networkId->asInt()).second)
                palette.mStateNames.emplace(name->asString(), states->getKeys());
        }
        return palette;
    }

    int32_t olderHash(const OlderPalette &palette, const std::string &name, const Tag &states) {
        const auto stateNames = palette.mStateNames.find(name);
        if (stateNames != palette.mStateNames.end()) {
            Tag kept = states;
            for (const std::string &key: states.getKeys()) {
                if (std::find(stateNames->second.begin(), stateNames->second.end(), key) == stateNames->second.end())
                    kept.remove(key);
            }

            const int32_t hash = BlockStateHasher::hash(name, kept);
            if (palette.mHashes.count(hash) != 0)
                return hash;
            return palette.mDefaultHashes.at(name);
        }

        const auto base = palette.mDefaultHashes.find(ItemNetworkIdTable::getVariantBase(name));
        if (base != palette.mDefaultHashes.end())
            return base->second;

        const auto air = palette.mDefaultHashes.find(AIR);
        return air == palette.mDefaultHashes.end() ? BlockStateHasher::hash(AIR) : air->second;
    }

    std::shared_ptr<const BlockNetworkIdMap> buildNetworkIds(const OlderPalette &palette) {
        BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
        registry.initialize();

        std::shared_ptr<BlockNetworkIdMap> map = std::make_shared<BlockNetworkIdMap>();
        for (const std::string &name: registry.getBlockNames()) {
            const std::vector<Tag> *permutations = registry.getPermutations(name);
            if (permutations == nullptr)
                continue;

            for (const Tag &states: *permutations) {
                const int32_t hash = BlockStateHasher::hash(name, states);
                if (palette.mHashes.count(hash) == 0)
                    map->add(hash, olderHash(palette, name, states));
            }
        }
        return map;
    }
}

std::shared_ptr<const BlockNetworkIdMap> BlockNetworkIdTable::getNetworkIds(int32_t protocol) {
    const ProtocolData *data = ProtocolData::find(protocol);
    if (data == nullptr)
        return nullptr;

    static std::mutex mutex;
    static std::unordered_map<int32_t, std::shared_ptr<const BlockNetworkIdMap>> maps;

    const std::lock_guard<std::mutex> lock(mutex);
    std::shared_ptr<const BlockNetworkIdMap> &map = maps[data->mProtocol];
    if (map == nullptr)
        map = buildNetworkIds(loadPalette(data->mBlockPalette, data->mBlockPaletteSize));
    return map;
}
