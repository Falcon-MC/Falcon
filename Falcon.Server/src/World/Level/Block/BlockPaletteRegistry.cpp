#include "World/Level/Block/BlockPaletteRegistry.h"

#include "BlockPaletteNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <algorithm>
#include <future>
#include <thread>
#include <utility>

namespace {
    // the other startup registries load on their own threads meanwhile
    const size_t MAX_PARSE_THREADS = 4;

    struct Cursor {
        const std::string &mBytes;
        size_t mOffset;

        void skip(size_t length) {
            if (mBytes.size() - mOffset < length)
                throw NbtException("Truncated block palette");
            mOffset += length;
        }

        uint32_t readBigEndian(size_t width) {
            const size_t start = mOffset;
            skip(width);
            uint32_t value = 0;
            for (size_t i = 0; i < width; ++i)
                value = (value << 8) | (unsigned char) mBytes[start + i];
            return value;
        }

        size_t readLength() {
            const int32_t length = (int32_t) readBigEndian(4);
            if (length < 0)
                throw NbtException("Invalid block palette length");
            return (size_t) length;
        }
    };

    // walks a value without building it: only finds where each entry ends, NbtIo still parses and checks it
    void skipValue(Cursor &cursor, Tag::Type type, int maxDepth) {
        if (maxDepth < 0)
            throw NbtException("Block palette is too deeply nested");

        switch (type) {
            case Tag::Type::End:
                return;
            case Tag::Type::Byte:
                cursor.skip(1);
                return;
            case Tag::Type::Short:
                cursor.skip(2);
                return;
            case Tag::Type::Int:
            case Tag::Type::Float:
                cursor.skip(4);
                return;
            case Tag::Type::Long:
            case Tag::Type::Double:
                cursor.skip(8);
                return;
            case Tag::Type::ByteArray:
                cursor.skip(cursor.readLength());
                return;
            case Tag::Type::String:
                cursor.skip(cursor.readBigEndian(2));
                return;
            case Tag::Type::IntArray:
                cursor.skip(cursor.readLength() * 4);
                return;
            case Tag::Type::LongArray:
                cursor.skip(cursor.readLength() * 8);
                return;
            case Tag::Type::List: {
                const unsigned char elementType = (unsigned char) cursor.readBigEndian(1);
                if (!Tag::isValidType(elementType))
                    throw NbtException("Invalid block palette tag type");
                const size_t length = cursor.readLength();
                for (size_t i = 0; i < length; ++i)
                    skipValue(cursor, (Tag::Type) elementType, maxDepth - 1);
                return;
            }
            case Tag::Type::Compound:
                for (;;) {
                    const unsigned char childType = (unsigned char) cursor.readBigEndian(1);
                    if (!Tag::isValidType(childType))
                        throw NbtException("Invalid block palette tag type");
                    if ((Tag::Type) childType == Tag::Type::End)
                        return;
                    cursor.skip(cursor.readBigEndian(2));
                    skipValue(cursor, (Tag::Type) childType, maxDepth - 1);
                }
        }
    }

    // consecutive permutations of one block: merged with one lookup instead of one per permutation
    struct BlockRun {
        std::string mName;
        Tag mFirstStates;
        bool mHasNetworkId = false;
        Tag mNetworkId;
        std::vector<Tag> mPermutations;
    };

    std::vector<BlockRun> readRuns(std::string bytes, const EncodingSettings &settings, size_t count) {
        ReadOnlyBinaryStream stream(std::move(bytes));
        stream.setEncodingSettings(settings);
        std::vector<BlockRun> runs;
        for (size_t i = 0; i < count; ++i) {
            Tag entry = NbtIo::readValue(stream, Tag::Type::Compound, NbtVariant::BigEndian, NbtIo::MAX_DEPTH - 2);
            const Tag *nameTag = entry.get("name");
            Tag *statesTag = entry.get("states");
            if (nameTag == nullptr || statesTag == nullptr)
                continue;

            const std::string &name = nameTag->asString();
            if (runs.empty() || runs.back().mName != name) {
                runs.emplace_back();
                BlockRun &run = runs.back();
                run.mName = name;
                run.mFirstStates = *statesTag;
                // read as an int only when the merge keeps it: the first permutation of the whole palette
                if (const Tag *networkIdTag = entry.get("network_id")) {
                    run.mHasNetworkId = true;
                    run.mNetworkId = *networkIdTag;
                }
            }
            // states moved out: no deep copy per permutation
            runs.back().mPermutations.push_back(std::move(*statesTag));
        }
        if (!stream.feof())
            throw NbtException("Block palette entry boundaries do not match");
        return runs;
    }
}

BlockPaletteRegistry::BlockPaletteRegistry() : mLoaded(false) {}

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

            const std::string &bytes = stream.getBuffer();
            std::vector<size_t> offsets((size_t) count + 1);
            Cursor cursor{bytes, stream.getOffset()};
            for (int32_t i = 0; i < count; ++i) {
                offsets[(size_t) i] = cursor.mOffset;
                skipValue(cursor, Tag::Type::Compound, NbtIo::MAX_DEPTH - 2);
            }
            offsets[(size_t) count] = cursor.mOffset;

            // tag building dominates startup: ranges parse on their own threads, merged below in file order
            const size_t hardware = std::max<unsigned>(1, std::thread::hardware_concurrency());
            const size_t parts = std::max<size_t>(1, std::min({MAX_PARSE_THREADS, hardware, (size_t) count}));
            std::vector<std::future<std::vector<BlockRun>>> ranges;
            for (size_t part = 0; part < parts; ++part) {
                const size_t first = (size_t) count * part / parts;
                const size_t last = (size_t) count * (part + 1) / parts;
                std::string range = bytes.substr(offsets[first], offsets[last] - offsets[first]);
                ranges.push_back(std::async(part == 0 ? std::launch::deferred : std::launch::async, readRuns,
                                            std::move(range), trustedSettings, last - first));
            }

            for (std::future<std::vector<BlockRun>> &range: ranges) {
                std::vector<BlockRun> runs = range.get();
                for (BlockRun &run: runs) {
                    if (palette.mDefaultStates.find(run.mName) == palette.mDefaultStates.end()) {
                        palette.mBlockNames.push_back(run.mName);
                        palette.mDefaultStates.emplace(run.mName, std::move(run.mFirstStates));
                        if (run.mHasNetworkId)
                            palette.mDefaultNetworkId.emplace(run.mName, run.mNetworkId.asInt());
                    }
                    std::vector<Tag> &permutations = palette.mPermutations[run.mName];
                    if (permutations.empty()) {
                        permutations = std::move(run.mPermutations);
                        continue;
                    }
                    for (Tag &states: run.mPermutations)
                        permutations.push_back(std::move(states));
                }
            }
            stream.setOffset(offsets[(size_t) count]);
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
