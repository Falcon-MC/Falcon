#include "World/Level/Storage/LevelStorage.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/Chunk/SubChunk.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Storage/FalconDataVersion.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>

#include <leveldb/db.h>
#include <leveldb/iterator.h>
#include <leveldb/decompress_allocator.h>
#include <leveldb/options.h>
#include <leveldb/write_batch.h>
#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>

namespace {
    const char *WEATHER_KEY = "weather";
    const size_t LEVEL_DAT_HEADER_SIZE = 8;
    const char *GAME_RULES_KEY = "gameRules";
    const int32_t FINALIZED_STATE_NEEDS_POPULATION = 1;
    const int32_t FINALIZED_STATE_DONE = 2;
    const char *PENDING_BLOCK_CHANGES_PREFIX = "falcon_pending_block_changes";
    // an NBT root starts with its tag type, never 0xff: tells the compact format from the old one
    const unsigned char PENDING_CHANGES_COMPACT_MARKER = 0xff;
    const unsigned char PENDING_CHANGES_COMPACT_VERSION = 1;
    const std::string ACTOR_PREFIX = "actorprefix";
    const std::string ACTOR_DIGEST_PREFIX = "digp";
    const char *ACTOR_UNIQUE_ID_TAG = "UniqueID";
    const size_t ACTOR_STORAGE_ID_SIZE = 8;
    const std::string TICKING_AREA_PREFIX = "tickingarea_";
    const std::string MAP_PREFIX = "map_";

    int64_t packBlockPosition(int32_t x, int32_t y, int32_t z) {
        return (((int64_t) (x + 30000000) & 0x3FFFFFFLL) << 37) | (((int64_t) (z + 30000000) & 0x3FFFFFFLL) << 11)
               | (((int64_t) (y + 400) & 0x3FFLL) << 1);
    }

    const EncodingSettings &_storageEncodingSettings() {
        static const EncodingSettings settings = []() {
            EncodingSettings storage;
            storage.mMaxListSize = 1024 * 1024;
            storage.mMaxByteArraySize = 32 * 1024 * 1024;
            storage.mMaxStringLength = 1024 * 32;
            return storage;
        }();

        return settings;
    }

    struct OpenedDatabase {
        leveldb::Status mStatus;
        std::shared_ptr<const leveldb::FilterPolicy> mFilterPolicy;
        std::shared_ptr<leveldb::Cache> mBlockCache;
        // last: released before the cache and filter it reads
        std::shared_ptr<leveldb::DB> mDb;
    };

    std::filesystem::path levelRoot(const std::string &worldsDirectory, const std::string &levelName) {
        return std::filesystem::path(worldsDirectory) / levelName;
    }

    OpenedDatabase openDatabase(const std::filesystem::path &root) {
        std::error_code error;
        std::filesystem::create_directories(root / "db", error);

        leveldb::Options options;
        options.create_if_missing = true;
        // raw zlib: the game's block compression, worlds stay interchangeable
        options.compression = leveldb::kZlibRawCompression;
        options.block_size = 163840;
        // chunk keys span the whole key space: every flushed table overlaps all of level 1 and rewrites it;
        // a 4 MB memtable (0.5 MB compressed) let compaction fall behind and stalled auto-saves for seconds
        options.write_buffer_size = 32 * 1024 * 1024;

        OpenedDatabase opened;
        opened.mFilterPolicy.reset(leveldb::NewBloomFilterPolicy(10));
        opened.mBlockCache.reset(leveldb::NewLRUCache(8 * 1024 * 1024));
        options.filter_policy = opened.mFilterPolicy.get();
        options.block_cache = opened.mBlockCache.get();

        leveldb::DB *db = nullptr;
        opened.mStatus = leveldb::DB::Open(options, (root / "db").string(), &db);
        opened.mDb.reset(db);
        return opened;
    }

    // a database opens with log recovery and synced writes: started while the registries load
    std::mutex preopenedMutex;
    std::string preopenedPath;
    std::future<OpenedDatabase> preopened;

    std::future<OpenedDatabase> takePreopened(const std::string &path) {
        std::lock_guard<std::mutex> lock(preopenedMutex);
        if (!preopened.valid() || preopenedPath != path)
            return std::future<OpenedDatabase>();
        preopenedPath.clear();
        return std::move(preopened);
    }
}

LevelStorage::LevelStorage() : mDimensionId(0) {}

LevelStorage::~LevelStorage() {
    close();
}

LevelStorage::LevelStorage(LevelStorage &&other) noexcept
        : mFilterPolicy(std::move(other.mFilterPolicy)), mBlockCache(std::move(other.mBlockCache)),
          mDb(std::move(other.mDb)), mDecompressAllocator(std::move(other.mDecompressAllocator)),
          mPath(std::move(other.mPath)), mDimensionId(other.mDimensionId) {
    other.mDb.reset();
    other.mDecompressAllocator.reset();
    other.mBlockCache.reset();
    other.mFilterPolicy.reset();
}

LevelStorage &LevelStorage::operator=(LevelStorage &&other) noexcept {
    if (this == &other)
        return *this;

    close();

    mFilterPolicy = std::move(other.mFilterPolicy);
    mBlockCache = std::move(other.mBlockCache);
    mDb = std::move(other.mDb);
    mDecompressAllocator = std::move(other.mDecompressAllocator);
    mPath = std::move(other.mPath);
    mDimensionId = other.mDimensionId;
    other.mDb.reset();
    other.mDecompressAllocator.reset();
    other.mBlockCache.reset();
    other.mFilterPolicy.reset();

    return *this;
}

bool LevelStorage::attach(const LevelStorage &source, int dimensionId) {
    if (source.mDb == nullptr)
        return false;

    close();

    mFilterPolicy = source.mFilterPolicy;
    mBlockCache = source.mBlockCache;
    mDb = source.mDb;
    mDecompressAllocator = source.mDecompressAllocator;
    mPath = source.mPath;
    mDimensionId = dimensionId;

    return true;
}

void LevelStorage::_appendLInt(std::string &out, int32_t value) {
    out.push_back((char) (value & 0xff));
    out.push_back((char) ((value >> 8) & 0xff));
    out.push_back((char) ((value >> 16) & 0xff));
    out.push_back((char) ((value >> 24) & 0xff));
}

std::string LevelStorage::_makeKey(int32_t chunkX, int32_t chunkZ, LevelDbTag tag) const {
    // x, z as LE ints, dimension id unless overworld, then the record tag byte
    std::string key;
    _appendLInt(key, chunkX);
    _appendLInt(key, chunkZ);

    if (mDimensionId != 0)
        _appendLInt(key, mDimensionId);

    key.push_back((char) tag);
    return key;
}

std::string LevelStorage::_makeDataVersionKey(int32_t chunkX, int32_t chunkZ) const {
    std::string key;
    _appendLInt(key, chunkX);
    _appendLInt(key, chunkZ);

    if (mDimensionId != 0)
        _appendLInt(key, mDimensionId);

    key += FalconDataVersion::TAG;
    return key;
}

int LevelStorage::_firstBiomeSection() const {
    const bool overworldRange = mDimensionId == static_cast<int>(DimensionType::Overworld)
                                || Dimension::isCustom(static_cast<DimensionType>(mDimensionId));
    return overworldRange ? 0 : -LevelChunk::LOWEST_SUB_CHUNK_Y;
}

int LevelStorage::_biomeSectionCount() const {
    // the game stores the sections of a dimension's own build range, from its bottom
    if (mDimensionId == static_cast<int>(DimensionType::Nether))
        return 8;
    if (mDimensionId == static_cast<int>(DimensionType::TheEnd))
        return 16;
    return LevelChunk::SUB_CHUNK_COUNT;
}

std::string LevelStorage::_makeSubChunkKey(int32_t chunkX, int32_t chunkZ, int8_t subY) const {
    std::string key = _makeKey(chunkX, chunkZ, LevelDbTag::SubChunkPrefix);
    key.push_back((char) subY);
    return key;
}

void LevelStorage::preopen(const std::string &worldsDirectory, const std::string &levelName) {
    discardPreopened();

    const std::filesystem::path root = levelRoot(worldsDirectory, levelName);
    // existing worlds only: create_if_missing would leave an empty world behind if startup stops before opening it
    std::error_code missing;
    if (!std::filesystem::exists(root / "db", missing))
        return;

    std::lock_guard<std::mutex> lock(preopenedMutex);
    try {
        preopened = std::async(std::launch::async, openDatabase, root);
        preopenedPath = (root / "db").string();
    } catch (const std::system_error &) {
        // no thread to spare: open() opens it itself
        preopenedPath.clear();
    }
}

void LevelStorage::discardPreopened() {
    std::future<OpenedDatabase> unclaimed;
    {
        std::lock_guard<std::mutex> lock(preopenedMutex);
        unclaimed = std::move(preopened);
        preopenedPath.clear();
    }
    if (unclaimed.valid())
        unclaimed.get();
}

bool LevelStorage::open(const std::string &worldsDirectory, const std::string &levelName, int dimensionId) {
    close();

    mDimensionId = dimensionId;

    std::error_code error;
    const std::filesystem::path root = levelRoot(worldsDirectory, levelName);
    std::filesystem::create_directories(root / "db", error);

    mPath = (root / "db").string();

    LOG_INFO(LogAreaID::Server, "Opening level '%s'", mPath.c_str());

    std::future<OpenedDatabase> early = takePreopened(mPath);
    const OpenedDatabase opened = early.valid() ? early.get() : openDatabase(root);

    if (!opened.mStatus.ok()) {
        LOG_ERROR(LogAreaID::Server, "Could not open level database %s: %s", mPath.c_str(),
                  opened.mStatus.ToString().c_str());
        return false;
    }

    mFilterPolicy = opened.mFilterPolicy;
    mBlockCache = opened.mBlockCache;
    mDb = opened.mDb;
    mDecompressAllocator = std::make_shared<leveldb::DecompressAllocator>();

    const std::filesystem::path nameFile = root / "levelname.txt";
    if (!std::filesystem::exists(nameFile, error)) {
        std::ofstream file(nameFile);
        if (file.is_open())
            file << levelName;
    }

    return true;
}

void LevelStorage::close() {
    if (mDb == nullptr)
        return;

    mDb.reset();
    mDecompressAllocator.reset();
    mBlockCache.reset();
    mFilterPolicy.reset();
}

leveldb::ReadOptions LevelStorage::_readOptions() const {
    leveldb::ReadOptions options;
    options.decompress_allocator = mDecompressAllocator.get();
    return options;
}

void LevelStorage::compact() {
    if (mDb == nullptr)
        return;

    mDb->CompactRange(nullptr, nullptr);
}

bool LevelStorage::saveChunk(const LevelChunk &chunk) {
    if (mDb == nullptr)
        return false;

    // one batch: a crash mid-save never mixes old and new sub-chunks
    leveldb::WriteBatch batch;

    const std::string version(1, (char) LevelChunk::STORAGE_VERSION);
    batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Version), version);

    std::string dataVersion;
    _appendLInt(dataVersion, (int32_t) (FalconDataVersion::CURRENT & 0xffffffff));
    _appendLInt(dataVersion, (int32_t) (FalconDataVersion::CURRENT >> 32));
    batch.Put(_makeDataVersionKey(chunk.getX(), chunk.getZ()), dataVersion);

    std::string finalized;
    _appendLInt(finalized, chunk.isPopulated() ? FINALIZED_STATE_DONE : FINALIZED_STATE_NEEDS_POPULATION);
    batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::FinalizedState), finalized);
    if (chunk.getPendingTicks().empty())
        batch.Delete(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::PendingTicks));
    else
        batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::PendingTicks), chunk.getPendingTicks());
    if (chunk.getRandomTicks().empty())
        batch.Delete(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::RandomTicks));
    else
        batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::RandomTicks), chunk.getRandomTicks());
    // sub_1406D1BC0: written while an event waits, deleted otherwise
    if (chunk.getNamedTicks().empty())
        batch.Delete(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::NamedTicks));
    else
        batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::NamedTicks), chunk.getNamedTicks());

    for (int i = 0; i < LevelChunk::SUB_CHUNK_COUNT; i++) {
        const SubChunk &subChunk = chunk.getSubChunk(i);

        BinaryStream stream;
        subChunk.writePersistent(stream);

        batch.Put(_makeSubChunkKey(chunk.getX(), chunk.getZ(), subChunk.getY()), stream.getBuffer());
    }

    // Data3D starts with a 256-entry 16-bit heightmap: zeros, loadChunk skips those 512 bytes
    BinaryStream heightAndBiomes;
    for (int i = 0; i < 256; i++) {
        heightAndBiomes.putByte(0);
        heightAndBiomes.putByte(0);
    }
    heightAndBiomes.put(chunk.encodeBiomesPersistent(_firstBiomeSection(), _biomeSectionCount()));
    batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Data3D), heightAndBiomes.getBuffer());

    const leveldb::Status status = mDb->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save chunk %d %d: %s", chunk.getX(), chunk.getZ(),
                 status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::hasChunk(int32_t chunkX, int32_t chunkZ) const {
    if (mDb == nullptr)
        return false;

    std::string value;
    const LevelDbTag tags[] = {LevelDbTag::Version, LevelDbTag::LegacyVersion, LevelDbTag::LegacyTerrain};
    for (const LevelDbTag tag: tags) {
        if (mDb->Get(_readOptions(), _makeKey(chunkX, chunkZ, tag), &value).ok())
            return true;
    }

    return false;
}

bool LevelStorage::loadChunk(LevelChunk &chunk) {
    if (mDb == nullptr)
        return false;

    std::string version;
    leveldb::Status status = mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Version),
                                      &version);

    if (!status.ok()) {
        status = mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::LegacyVersion), &version);
    }

    if (!status.ok())
        return false;

    bool loadedAny = false;
    bool replacedUnknown = false;

    for (int i = 0; i < LevelChunk::SUB_CHUNK_COUNT; i++) {
        const int8_t subY = (int8_t) (LevelChunk::LOWEST_SUB_CHUNK_Y + i);

        std::string data;
        if (!mDb->Get(_readOptions(), _makeSubChunkKey(chunk.getX(), chunk.getZ(), subY), &data).ok())
            continue;

        ReadOnlyBinaryStream stream(data);
        stream.setEncodingSettings(_storageEncodingSettings());

        try {
            if (chunk.getSubChunk(i).readPersistent(stream, &replacedUnknown))
                loadedAny = true;
        } catch (const BinaryDataException &exception) {
            LOG_WARN(LogAreaID::Server, "Malformed sub chunk %d %d %d: %s", chunk.getX(), chunk.getZ(), (int) subY,
                     exception.what());
        }
    }

    std::string heightAndBiomes;
    if (mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Data3D), &heightAndBiomes).ok()) {
        ReadOnlyBinaryStream stream(heightAndBiomes);

        try {
            for (int i = 0; i < 512; i++)
                stream.getByte();

            chunk.readBiomesPersistent(stream, _firstBiomeSection(), _biomeSectionCount());
        } catch (const BinaryDataException &exception) {
            LOG_WARN(LogAreaID::Server, "Malformed biome data for chunk %d %d: %s", chunk.getX(), chunk.getZ(),
                     exception.what());
        }
    }

    std::string finalized;
    if (mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::FinalizedState), &finalized).ok()
        && finalized.size() >= 4) {
        const int32_t state = (int32_t) ((unsigned char) finalized[0] | ((unsigned char) finalized[1] << 8)
                                         | ((unsigned char) finalized[2] << 16) | ((unsigned char) finalized[3] << 24));
        chunk.setPopulated(state >= FINALIZED_STATE_DONE);
    }

    std::string pendingTicks;
    if (mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::PendingTicks), &pendingTicks).ok())
        chunk.setPendingTicks(std::move(pendingTicks));

    std::string randomTicks;
    if (mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::RandomTicks), &randomTicks).ok())
        chunk.setRandomTicks(std::move(randomTicks));

    std::string namedTicks;
    if (mDb->Get(_readOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::NamedTicks), &namedTicks).ok())
        chunk.setNamedTicks(std::move(namedTicks));

    // no version: written by the game, never migrated
    std::string dataVersion;
    if (mDb->Get(_readOptions(), _makeDataVersionKey(chunk.getX(), chunk.getZ()), &dataVersion).ok()
        && dataVersion.size() >= 8) {
        uint64_t value = 0;
        for (int i = 7; i >= 0; --i)
            value = value << 8 | (unsigned char) dataVersion[(size_t) i];
        chunk.setNeedsLiquidMigration((int64_t) value < FalconDataVersion::STILL_LIQUIDS);
    }

    chunk.clearDirty();
    if (replacedUnknown)
        chunk.markDirty();
    return loadedAny;
}

std::string LevelStorage::_makePendingChangesKey(int32_t chunkX, int32_t chunkZ) const {
    std::string key = PENDING_BLOCK_CHANGES_PREFIX;
    _appendLInt(key, chunkX);
    _appendLInt(key, chunkZ);
    _appendLInt(key, mDimensionId);
    return key;
}

bool LevelStorage::mergePendingBlockChanges(int32_t chunkX, int32_t chunkZ,
                                            const std::vector<GeneratedBlockChange> &changes) {
    if (mDb == nullptr || changes.empty())
        return false;

    std::vector<GeneratedBlockChange> merged = loadPendingBlockChanges(chunkX, chunkZ);
    std::unordered_map<int64_t, size_t> index;

    for (size_t position = 0; position < merged.size(); position++)
        index[packBlockPosition(merged[position].mX, merged[position].mY, merged[position].mZ)
              | merged[position].mLayer] = position;

    for (const GeneratedBlockChange &change: changes) {
        const int64_t packed = packBlockPosition(change.mX, change.mY, change.mZ) | change.mLayer;
        const std::unordered_map<int64_t, size_t>::const_iterator found = index.find(packed);

        if (found != index.end()) {
            merged[found->second] = change;
            continue;
        }

        index[packed] = merged.size();
        merged.push_back(change);
    }

    // states once, changes by index: thousands of spilled blocks of a few states; one compound each would
    // rewrite megabytes per save
    BinaryStream stream;
    stream.putByte(PENDING_CHANGES_COMPACT_MARKER);
    stream.putByte(PENDING_CHANGES_COMPACT_VERSION);

    std::unordered_map<const BlockState *, uint32_t> paletteIndex;
    std::vector<const BlockState *> palette;
    for (const GeneratedBlockChange &change: merged) {
        if (paletteIndex.emplace(change.mState, (uint32_t) palette.size()).second)
            palette.push_back(change.mState);
    }

    stream.putUnsignedVarInt((uint32_t) palette.size());
    for (const BlockState *state: palette)
        NbtIo::writeTag(stream, state->toNbt(), NbtVariant::LittleEndian);

    stream.putUnsignedVarInt((uint32_t) merged.size());
    for (const GeneratedBlockChange &change: merged) {
        stream.putVarInt(change.mX - chunkX * 16);
        stream.putVarInt(change.mY);
        stream.putVarInt(change.mZ - chunkZ * 16);
        stream.putByte((unsigned char) (change.mLayer == 1 ? 1 : 0));
        stream.putUnsignedVarInt(paletteIndex[change.mState]);
    }

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), _makePendingChangesKey(chunkX, chunkZ),
                                            stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save pending block changes for chunk %d %d: %s", chunkX, chunkZ,
                 status.ToString().c_str());
        return false;
    }

    return true;
}

std::vector<GeneratedBlockChange> LevelStorage::loadPendingBlockChanges(int32_t chunkX, int32_t chunkZ) {
    std::vector<GeneratedBlockChange> changes;

    if (mDb == nullptr)
        return changes;

    std::string data;
    if (!mDb->Get(_readOptions(), _makePendingChangesKey(chunkX, chunkZ), &data).ok())
        return changes;

    ReadOnlyBinaryStream stream(data);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
        if (!data.empty() && (unsigned char) data[0] == PENDING_CHANGES_COMPACT_MARKER) {
            stream.getByte();
            if (stream.getByte() != PENDING_CHANGES_COMPACT_VERSION)
                throw std::runtime_error("unknown pending change version");

            const uint32_t paletteSize = stream.getUnsignedVarInt();
            if (paletteSize > stream.getRemainingLength())
                throw std::runtime_error("pending change palette overruns the record");

            std::vector<const BlockState *> palette(paletteSize);
            for (const BlockState *&state: palette) {
                const Tag block = NbtIo::readTag(stream, NbtVariant::LittleEndian);
                const Tag *states = block.get("states");
                state = SubChunk::intern(BlockState(block.getString("name", "minecraft:air"),
                                                    states == nullptr ? Tag::ofCompound() : *states));
            }

            const uint32_t count = stream.getUnsignedVarInt();
            if (count > stream.getRemainingLength())
                throw std::runtime_error("pending changes overrun the record");

            changes.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                GeneratedBlockChange change;
                change.mX = chunkX * 16 + stream.getVarInt();
                change.mY = stream.getVarInt();
                change.mZ = chunkZ * 16 + stream.getVarInt();
                change.mLayer = stream.getByte() == 1 ? 1 : 0;
                const uint32_t index = stream.getUnsignedVarInt();
                if (index >= palette.size())
                    throw std::runtime_error("pending change state out of range");
                change.mState = palette[index];
                changes.push_back(change);
            }
            return changes;
        }

        // worlds saved before the compact format hold one NBT compound per change
        const Tag root = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        const Tag *list = root.get("changes");
        if (list == nullptr || !list->isList())
            return changes;

        for (const Tag &entry: list->getList()) {
            if (!entry.isCompound())
                continue;

            const Tag *block = entry.get("block");
            if (block == nullptr || !block->isCompound())
                continue;

            const Tag *states = block->get("states");

            GeneratedBlockChange change;
            change.mX = entry.getInt("x");
            change.mY = entry.getInt("y");
            change.mZ = entry.getInt("z");
            change.mLayer = entry.getInt("layer", 0) == 1 ? 1 : 0;
            change.mState = SubChunk::intern(BlockState(block->getString("name", "minecraft:air"),
                                                        states == nullptr ? Tag::ofCompound() : *states));
            changes.push_back(change);
        }
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed pending block changes for chunk %d %d: %s", chunkX, chunkZ,
                 exception.what());
    }

    return changes;
}

bool LevelStorage::erasePendingBlockChanges(int32_t chunkX, int32_t chunkZ) {
    if (mDb == nullptr)
        return false;

    const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), _makePendingChangesKey(chunkX, chunkZ));
    return _recordWrite(status.ok() || status.IsNotFound());
}

std::string LevelStorage::_makeActorDigestKey(int32_t chunkX, int32_t chunkZ) const {
    std::string key = ACTOR_DIGEST_PREFIX;
    _appendLInt(key, chunkX);
    _appendLInt(key, chunkZ);

    if (mDimensionId != 0)
        _appendLInt(key, mDimensionId);

    return key;
}

std::string LevelStorage::_makeActorStorageId(int64_t uniqueId) {
    const uint32_t startCount = (uint32_t) ((uint64_t) uniqueId >> 32);
    const uint32_t counter = (uint32_t) uniqueId;
    const uint32_t storedStart = 0u - startCount;

    std::string id;
    for (int shift = 24; shift >= 0; shift -= 8)
        id.push_back((char) ((storedStart >> shift) & 0xff));
    for (int shift = 24; shift >= 0; shift -= 8)
        id.push_back((char) ((counter >> shift) & 0xff));
    return id;
}

bool LevelStorage::saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities) {
    if (mDb == nullptr)
        return false;

    leveldb::WriteBatch batch;
    std::string digest;

    for (const Tag &entity: entities) {
        if (!entity.contains(ACTOR_UNIQUE_ID_TAG)) {
            LOG_WARN(LogAreaID::Server, "Skipped an entity without a unique id in chunk %d %d", chunkX, chunkZ);
            continue;
        }

        const std::string storageId = _makeActorStorageId(entity.getLong(ACTOR_UNIQUE_ID_TAG));
        BinaryStream stream;
        NbtIo::writeTag(stream, entity, NbtVariant::LittleEndian);
        batch.Put(ACTOR_PREFIX + storageId, stream.getBuffer());
        digest += storageId;
    }

    batch.Put(_makeActorDigestKey(chunkX, chunkZ), digest);
    batch.Delete(_makeKey(chunkX, chunkZ, LevelDbTag::LegacyEntities));

    const leveldb::Status status = mDb->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save entities for chunk %d %d: %s", chunkX, chunkZ,
                 status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::eraseEntity(int64_t uniqueId) {
    if (mDb == nullptr)
        return false;

    const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), ACTOR_PREFIX + _makeActorStorageId(uniqueId));
    return _recordWrite(status.ok() || status.IsNotFound());
}

bool LevelStorage::hasEntity(int64_t uniqueId) const {
    if (mDb == nullptr)
        return false;

    std::string data;
    return mDb->Get(_readOptions(), ACTOR_PREFIX + _makeActorStorageId(uniqueId), &data).ok();
}

std::vector<Tag> LevelStorage::loadEntities(int32_t chunkX, int32_t chunkZ) {
    std::vector<Tag> entities;

    if (mDb == nullptr)
        return entities;

    std::string digest;
    if (mDb->Get(_readOptions(), _makeActorDigestKey(chunkX, chunkZ), &digest).ok()) {
        for (size_t offset = 0; offset + ACTOR_STORAGE_ID_SIZE <= digest.size(); offset += ACTOR_STORAGE_ID_SIZE) {
            std::string data;
            if (!mDb->Get(_readOptions(), ACTOR_PREFIX + digest.substr(offset, ACTOR_STORAGE_ID_SIZE), &data).ok())
                continue;

            ReadOnlyBinaryStream stream(data);
            stream.setEncodingSettings(_storageEncodingSettings());

            try {
                entities.push_back(NbtIo::readTag(stream, NbtVariant::LittleEndian));
            } catch (const std::exception &exception) {
                LOG_WARN(LogAreaID::Server, "Malformed entity in chunk %d %d: %s", chunkX, chunkZ, exception.what());
            }
        }
    }

    std::string legacy;
    if (!mDb->Get(_readOptions(), _makeKey(chunkX, chunkZ, LevelDbTag::LegacyEntities), &legacy).ok())
        return entities;

    ReadOnlyBinaryStream stream(legacy);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
        while (!stream.feof())
            entities.push_back(NbtIo::readTag(stream, NbtVariant::LittleEndian));
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed legacy entities for chunk %d %d: %s", chunkX, chunkZ, exception.what());
    }

    return entities;
}

bool LevelStorage::saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities) {
    if (mDb == nullptr)
        return false;

    const std::string key = _makeKey(chunkX, chunkZ, LevelDbTag::BlockEntities);

    if (blockEntities.empty()) {
        const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), key);
        return _recordWrite(status.ok() || status.IsNotFound());
    }

    BinaryStream stream;
    for (const Tag &blockEntity: blockEntities)
        NbtIo::writeTag(stream, blockEntity, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), key, stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save block entities for chunk %d %d: %s", chunkX, chunkZ,
                 status.ToString().c_str());
        return false;
    }

    return true;
}

std::vector<Tag> LevelStorage::loadBlockEntities(int32_t chunkX, int32_t chunkZ) {
    std::vector<Tag> blockEntities;

    if (mDb == nullptr)
        return blockEntities;

    std::string data;
    const leveldb::Status status = mDb->Get(_readOptions(), _makeKey(chunkX, chunkZ, LevelDbTag::BlockEntities), &data);

    if (!status.ok())
        return blockEntities;

    ReadOnlyBinaryStream stream(data);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
        while (!stream.feof())
            blockEntities.push_back(NbtIo::readTag(stream, NbtVariant::LittleEndian));
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed block entities for chunk %d %d: %s", chunkX, chunkZ, exception.what());
    }

    return blockEntities;
}

bool LevelStorage::saveWeather(bool raining, int32_t rainTime, bool thundering, int32_t thunderTime) {
    if (mDb == nullptr)
        return false;

    Tag weather = Tag::ofCompound();
    weather.putByte("raining", raining ? 1 : 0);
    weather.putInt("rainTime", rainTime);
    weather.putByte("thundering", thundering ? 1 : 0);
    weather.putInt("thunderTime", thunderTime);

    BinaryStream stream;
    NbtIo::writeTag(stream, weather, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), WEATHER_KEY, stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save weather: %s", status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::loadWeather(bool &raining, int32_t &rainTime, bool &thundering, int32_t &thunderTime) {
    if (mDb == nullptr)
        return false;

    std::string data;
    if (!mDb->Get(_readOptions(), WEATHER_KEY, &data).ok())
        return false;

    ReadOnlyBinaryStream stream(data);

    try {
        const Tag weather = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        raining = weather.getByte("raining") != 0;
        rainTime = weather.getInt("rainTime");
        thundering = weather.getByte("thundering") != 0;
        thunderTime = weather.getInt("thunderTime");
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed weather data: %s", exception.what());
        return false;
    }

    return true;
}

bool LevelStorage::saveTickingArea(const std::string &id, const Tag &area) {
    if (mDb == nullptr)
        return false;

    BinaryStream stream;
    NbtIo::writeTag(stream, area, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), TICKING_AREA_PREFIX + id, stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save ticking area %s: %s", id.c_str(), status.ToString().c_str());
        return false;
    }

    return true;
}

std::vector<std::pair<std::string, Tag>> LevelStorage::loadSavedDataWithPrefix(const std::string &prefix) {
    std::vector<std::pair<std::string, Tag>> entries;
    if (mDb == nullptr)
        return entries;

    std::unique_ptr<leveldb::Iterator> iterator(mDb->NewIterator(_readOptions()));
    for (iterator->Seek(prefix); iterator->Valid(); iterator->Next()) {
        const std::string key = iterator->key().ToString();
        if (key.compare(0, prefix.size(), prefix) != 0)
            break;

        ReadOnlyBinaryStream stream(iterator->value().ToString());
        try {
            entries.emplace_back(key.substr(prefix.size()), NbtIo::readTag(stream, NbtVariant::LittleEndian));
        } catch (const std::exception &exception) {
            LOG_WARN(LogAreaID::Server, "Malformed %s: %s", key.c_str(), exception.what());
        }
    }
    return entries;
}

bool LevelStorage::eraseSavedData(const std::string &key) {
    if (mDb == nullptr)
        return false;

    const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), key);
    return _recordWrite(status.ok() || status.IsNotFound());
}

bool LevelStorage::eraseTickingArea(const std::string &id) {
    if (mDb == nullptr)
        return false;

    const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), TICKING_AREA_PREFIX + id);
    return _recordWrite(status.ok() || status.IsNotFound());
}

std::vector<std::pair<std::string, Tag>> LevelStorage::loadTickingAreas() {
    std::vector<std::pair<std::string, Tag>> areas;
    if (mDb == nullptr)
        return areas;

    std::unique_ptr<leveldb::Iterator> iterator(mDb->NewIterator(_readOptions()));
    for (iterator->Seek(TICKING_AREA_PREFIX); iterator->Valid(); iterator->Next()) {
        const std::string key = iterator->key().ToString();
        if (key.compare(0, TICKING_AREA_PREFIX.size(), TICKING_AREA_PREFIX) != 0)
            break;

        ReadOnlyBinaryStream stream(iterator->value().ToString());
        try {
            areas.emplace_back(key.substr(TICKING_AREA_PREFIX.size()),
                               NbtIo::readTag(stream, NbtVariant::LittleEndian));
        } catch (const std::exception &exception) {
            LOG_WARN(LogAreaID::Server, "Malformed ticking area %s: %s", key.c_str(), exception.what());
        }
    }

    return areas;
}

bool LevelStorage::loadMapData(int64_t mapId, Tag &out) const {
    if (mDb == nullptr)
        return false;

    std::string data;
    if (!mDb->Get(_readOptions(), MAP_PREFIX + std::to_string(mapId), &data).ok())
        return false;

    ReadOnlyBinaryStream stream(data);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
        out = NbtIo::readTag(stream, NbtVariant::LittleEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed map data %lld: %s", (long long) mapId, exception.what());
        return false;
    }

    return out.isCompound();
}

bool LevelStorage::saveMapData(int64_t mapId, const Tag &data) {
    if (mDb == nullptr)
        return false;

    BinaryStream stream;
    NbtIo::writeTag(stream, data, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), MAP_PREFIX + std::to_string(mapId),
                                            stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save map %lld: %s", (long long) mapId, status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::eraseMapData(int64_t mapId) {
    if (mDb == nullptr)
        return false;

    const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), MAP_PREFIX + std::to_string(mapId));
    return _recordWrite(status.ok() || status.IsNotFound());
}

bool LevelStorage::saveGameRules(const Tag &rules) {
    if (mDb == nullptr)
        return false;

    BinaryStream stream;
    NbtIo::writeTag(stream, rules, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), GAME_RULES_KEY, stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save game rules: %s", status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::saveSavedData(const std::string &key, const Tag &data, bool wrapped) {
    if (mDb == nullptr)
        return false;

    Tag root = data;
    if (wrapped) {
        root = Tag::ofCompound();
        root.put("data", data);
    }

    BinaryStream stream;
    NbtIo::writeTag(stream, root, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), key, stream.getBuffer());
    if (!status.ok()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not save %s: %s", key.c_str(), status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::loadSavedData(const std::string &key, Tag &data, bool wrapped) {
    if (mDb == nullptr)
        return false;

    std::string raw;
    if (!mDb->Get(_readOptions(), key, &raw).ok())
        return false;

    ReadOnlyBinaryStream stream(raw);

    try {
        const Tag root = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        const Tag *stored = wrapped ? root.get("data") : &root;
        if (stored == nullptr || !stored->isCompound())
            return false;

        data = *stored;
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed %s data: %s", key.c_str(), exception.what());
        return false;
    }

    return true;
}

bool LevelStorage::loadGameRules(Tag &rules) {
    if (mDb == nullptr)
        return false;

    std::string data;
    if (!mDb->Get(_readOptions(), GAME_RULES_KEY, &data).ok())
        return false;

    ReadOnlyBinaryStream stream(data);

    try {
        rules = NbtIo::readTag(stream, NbtVariant::LittleEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed game rules data: %s", exception.what());
        return false;
    }

    return true;
}

bool LevelStorage::_recordWrite(bool success) const {
    if (!success)
        mWriteFailures.fetch_add(1);
    return success;
}

void LevelStorage::writeLevelDat(const std::string &levelName, int32_t spawnX, int32_t spawnY, int32_t spawnZ,
                                 int32_t gameType, int32_t difficulty, int64_t seed, int64_t time,
                                 bool bonusChestEnabled, bool bonusChestSpawned, int64_t worldStartCount) const {
    if (mPath.empty())
        return;

    const std::filesystem::path root = std::filesystem::path(mPath).parent_path();

    Tag data;
    if (!readLevelDat(data))
        data = Tag::ofCompound();
    if (!data.contains("LevelName"))
        data.putString("LevelName", levelName);
    if (!data.contains("GameType"))
        data.putInt("GameType", gameType);
    if (!data.contains("Difficulty"))
        data.putInt("Difficulty", difficulty);
    if (!data.contains("commandsEnabled"))
        data.putByte("commandsEnabled", 1);
    data.putInt("SpawnX", spawnX);
    data.putInt("SpawnY", spawnY);
    data.putInt("SpawnZ", spawnZ);
    data.putLong("RandomSeed", seed);
    data.putLong("Time", time);
    data.putInt("StorageVersion", 10);
    data.putInt("NetworkVersion", 2225);
    data.putByte("bonusChestEnabled", bonusChestEnabled ? 1 : 0);
    data.putByte("bonusChestSpawned", bonusChestSpawned ? 1 : 0);
    data.putInt("limitedWorldOriginX", spawnX);
    data.putInt("limitedWorldOriginY", spawnY);
    data.putInt("limitedWorldOriginZ", spawnZ);
    data.putLong("LastPlayed", (int64_t) std::time(nullptr));
    data.putLong("worldStartCount", worldStartCount);

    BinaryStream body;
    NbtIo::writeTag(body, data, NbtVariant::LittleEndian);

    const std::string payload = body.getBuffer();

    BinaryStream file;
    file.putLInt(10);
    file.putLInt((uint32_t) payload.size());
    file.put(payload);

    std::ofstream out((root / "level.dat").string(), std::ios::binary);
    if (!out.is_open()) {
        _recordWrite(false);
        LOG_WARN(LogAreaID::Server, "Could not write level.dat for %s", levelName.c_str());
        return;
    }

    const std::string bytes = file.getBuffer();
    out.write(bytes.data(), (std::streamsize) bytes.size());
    out.flush();
    if (!_recordWrite(out.good()))
        LOG_WARN(LogAreaID::Server, "Could not finish writing level.dat for %s", levelName.c_str());
}

bool LevelStorage::readLevelDat(Tag &out) const {
    if (mPath.empty())
        return false;

    const std::filesystem::path root = std::filesystem::path(mPath).parent_path();
    std::ifstream in((root / "level.dat").string(), std::ios::binary);
    if (!in.is_open())
        return false;

    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() <= LEVEL_DAT_HEADER_SIZE)
        return false;

    ReadOnlyBinaryStream stream(bytes.substr(LEVEL_DAT_HEADER_SIZE));

    try {
        out = NbtIo::readTag(stream, NbtVariant::LittleEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed level.dat: %s", exception.what());
        return false;
    }

    return out.isCompound();
}
