#include "Level/LevelStorage.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <utility>

#include <leveldb/db.h>
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

    int64_t packBlockPosition(int32_t x, int32_t y, int32_t z) {
        return (((int64_t) (x + 30000000) & 0x3FFFFFFLL) << 37)
               | (((int64_t) (z + 30000000) & 0x3FFFFFFLL) << 11)
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
}

LevelStorage::LevelStorage() : mDb(nullptr), mDimensionId(0) {}

LevelStorage::~LevelStorage() {
    close();
}

LevelStorage::LevelStorage(LevelStorage &&other) noexcept
        : mDb(std::move(other.mDb)), mPath(std::move(other.mPath)), mDimensionId(other.mDimensionId) {
    other.mDb.reset();
}

LevelStorage &LevelStorage::operator=(LevelStorage &&other) noexcept {
    if (this == &other)
        return *this;

    close();

    mDb = std::move(other.mDb);
    mPath = std::move(other.mPath);
    mDimensionId = other.mDimensionId;
    other.mDb.reset();

    return *this;
}

bool LevelStorage::attach(const LevelStorage &source, int dimensionId) {
    if (source.mDb == nullptr)
        return false;

    close();

    mDb = source.mDb;
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
    std::string key;
    _appendLInt(key, chunkX);
    _appendLInt(key, chunkZ);

    if (mDimensionId != 0)
        _appendLInt(key, mDimensionId);

    key.push_back((char) tag);
    return key;
}

std::string LevelStorage::_makeSubChunkKey(int32_t chunkX, int32_t chunkZ, int8_t subY) const {
    std::string key = _makeKey(chunkX, chunkZ, LevelDbTag::SubChunkPrefix);
    key.push_back((char) subY);
    return key;
}

bool LevelStorage::open(const std::string &worldsDirectory, const std::string &levelName, int dimensionId) {
    close();

    mDimensionId = dimensionId;

    std::error_code error;
    const std::filesystem::path root = std::filesystem::path(worldsDirectory) / levelName;
    std::filesystem::create_directories(root / "db", error);

    mPath = (root / "db").string();

    LOG_INFO(LogAreaID::Server, "Opening level '%s'", mPath.c_str());

    leveldb::Options options;
    options.create_if_missing = true;
    options.compression = leveldb::kZlibRawCompression;
    options.block_size = 163840;
    options.write_buffer_size = 4 * 1024 * 1024;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.block_cache = leveldb::NewLRUCache(8 * 1024 * 1024);

    leveldb::DB *db = nullptr;
    const leveldb::Status status = leveldb::DB::Open(options, mPath, &db);

    if (!status.ok()) {
        LOG_ERROR(LogAreaID::Server, "Could not open level database %s: %s", mPath.c_str(),
                  status.ToString().c_str());
        return false;
    }

    mDb.reset(db);

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
}

void LevelStorage::compact() {
    if (mDb == nullptr)
        return;

    mDb->CompactRange(nullptr, nullptr);
}

bool LevelStorage::saveChunk(const LevelChunk &chunk) {
    if (mDb == nullptr)
        return false;

    leveldb::WriteBatch batch;

    const std::string version(1, (char) LevelChunk::STORAGE_VERSION);
    batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Version), version);

    std::string finalized;
    _appendLInt(finalized, chunk.isPopulated() ? FINALIZED_STATE_DONE : FINALIZED_STATE_NEEDS_POPULATION);
    batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::FinalizedState), finalized);

    for (int i = 0; i < LevelChunk::SUB_CHUNK_COUNT; i++) {
        const SubChunk &subChunk = chunk.getSubChunk(i);

        BinaryStream stream;
        subChunk.writePersistent(stream);

        batch.Put(_makeSubChunkKey(chunk.getX(), chunk.getZ(), subChunk.getY()), stream.getBuffer());
    }

    BinaryStream heightAndBiomes;
    for (int i = 0; i < 256; i++) {
        heightAndBiomes.putByte(0);
        heightAndBiomes.putByte(0);
    }
    heightAndBiomes.put(chunk.encodeBiomesPersistent(LevelChunk::SUB_CHUNK_COUNT));
    batch.Put(_makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Data3D), heightAndBiomes.getBuffer());

    const leveldb::Status status = mDb->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        LOG_WARN(LogAreaID::Server, "Could not save chunk %d %d: %s", chunk.getX(), chunk.getZ(),
                 status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::loadChunk(LevelChunk &chunk) {
    if (mDb == nullptr)
        return false;

    std::string version;
    leveldb::Status status = mDb->Get(leveldb::ReadOptions(),
                                      _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Version), &version);

    if (!status.ok()) {
        status = mDb->Get(leveldb::ReadOptions(),
                          _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::LegacyVersion), &version);
    }

    if (!status.ok())
        return false;

    bool loadedAny = false;
    bool replacedUnknown = false;

    for (int i = 0; i < LevelChunk::SUB_CHUNK_COUNT; i++) {
        const int8_t subY = (int8_t) (LevelChunk::LOWEST_SUB_CHUNK_Y + i);

        std::string data;
        if (!mDb->Get(leveldb::ReadOptions(), _makeSubChunkKey(chunk.getX(), chunk.getZ(), subY), &data).ok())
            continue;

        ReadOnlyBinaryStream stream(data);
        stream.setEncodingSettings(_storageEncodingSettings());

        try {
            if (chunk.getSubChunk(i).readPersistent(stream, &replacedUnknown))
                loadedAny = true;
        } catch (const BinaryDataException &exception) {
            LOG_WARN(LogAreaID::Server, "Malformed sub chunk %d %d %d: %s", chunk.getX(), chunk.getZ(),
                     (int) subY, exception.what());
        }
    }

    std::string heightAndBiomes;
    if (mDb->Get(leveldb::ReadOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::Data3D),
                 &heightAndBiomes).ok()) {
        ReadOnlyBinaryStream stream(heightAndBiomes);

        try {
            for (int i = 0; i < 512; i++)
                stream.getByte();

            chunk.readBiomesPersistent(stream, LevelChunk::SUB_CHUNK_COUNT);
        } catch (const BinaryDataException &exception) {
            LOG_WARN(LogAreaID::Server, "Malformed biome data for chunk %d %d: %s", chunk.getX(), chunk.getZ(),
                     exception.what());
        }
    }

    std::string finalized;
    if (mDb->Get(leveldb::ReadOptions(), _makeKey(chunk.getX(), chunk.getZ(), LevelDbTag::FinalizedState),
                 &finalized).ok() && finalized.size() >= 4) {
        const int32_t state = (int32_t) ((unsigned char) finalized[0]
                                         | ((unsigned char) finalized[1] << 8)
                                         | ((unsigned char) finalized[2] << 16)
                                         | ((unsigned char) finalized[3] << 24));
        chunk.setPopulated(state >= FINALIZED_STATE_DONE);
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
        index[packBlockPosition(merged[position].mX, merged[position].mY, merged[position].mZ)] = position;

    for (const GeneratedBlockChange &change: changes) {
        const int64_t packed = packBlockPosition(change.mX, change.mY, change.mZ);
        const std::unordered_map<int64_t, size_t>::const_iterator found = index.find(packed);

        if (found != index.end()) {
            merged[found->second] = change;
            continue;
        }

        index[packed] = merged.size();
        merged.push_back(change);
    }

    Tag root = Tag::ofCompound();
    Tag list = Tag::ofList(Tag::Type::Compound);

    for (const GeneratedBlockChange &change: merged) {
        Tag entry = Tag::ofCompound();
        entry.putInt("x", change.mX);
        entry.putInt("y", change.mY);
        entry.putInt("z", change.mZ);
        entry.put("block", change.mState.toNbt());
        list.addToList(entry);
    }

    root.put("changes", list);

    BinaryStream stream;
    NbtIo::writeTag(stream, root, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), _makePendingChangesKey(chunkX, chunkZ),
                                            stream.getBuffer());
    if (!status.ok()) {
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
    if (!mDb->Get(leveldb::ReadOptions(), _makePendingChangesKey(chunkX, chunkZ), &data).ok())
        return changes;

    ReadOnlyBinaryStream stream(data);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
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
            change.mState = BlockState(block->getString("name", "minecraft:air"),
                                       states == nullptr ? Tag::ofCompound() : *states);
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

    const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(),
                                               _makePendingChangesKey(chunkX, chunkZ));
    return status.ok() || status.IsNotFound();
}

bool LevelStorage::saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities) {
    if (mDb == nullptr)
        return false;

    const std::string key = _makeKey(chunkX, chunkZ, LevelDbTag::Entities);

    if (entities.empty()) {
        const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), key);
        return status.ok() || status.IsNotFound();
    }

    BinaryStream stream;
    for (const Tag &entity: entities)
        NbtIo::writeTag(stream, entity, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), key, stream.getBuffer());
    if (!status.ok()) {
        LOG_WARN(LogAreaID::Server, "Could not save entities for chunk %d %d: %s", chunkX, chunkZ,
                 status.ToString().c_str());
        return false;
    }

    return true;
}

std::vector<Tag> LevelStorage::loadEntities(int32_t chunkX, int32_t chunkZ) {
    std::vector<Tag> entities;

    if (mDb == nullptr)
        return entities;

    std::string data;
    const leveldb::Status status = mDb->Get(leveldb::ReadOptions(),
                                            _makeKey(chunkX, chunkZ, LevelDbTag::Entities), &data);

    if (!status.ok())
        return entities;

    ReadOnlyBinaryStream stream(data);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
        while (!stream.feof())
            entities.push_back(NbtIo::readTag(stream, NbtVariant::LittleEndian));
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed entities for chunk %d %d: %s", chunkX, chunkZ,
                 exception.what());
    }

    return entities;
}

bool LevelStorage::saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities) {
    if (mDb == nullptr)
        return false;

    const std::string key = _makeKey(chunkX, chunkZ, LevelDbTag::BlockEntities);

    if (blockEntities.empty()) {
        const leveldb::Status status = mDb->Delete(leveldb::WriteOptions(), key);
        return status.ok() || status.IsNotFound();
    }

    BinaryStream stream;
    for (const Tag &blockEntity: blockEntities)
        NbtIo::writeTag(stream, blockEntity, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), key, stream.getBuffer());
    if (!status.ok()) {
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
    const leveldb::Status status = mDb->Get(leveldb::ReadOptions(),
                                            _makeKey(chunkX, chunkZ, LevelDbTag::BlockEntities), &data);

    if (!status.ok())
        return blockEntities;

    ReadOnlyBinaryStream stream(data);
    stream.setEncodingSettings(_storageEncodingSettings());

    try {
        while (!stream.feof())
            blockEntities.push_back(NbtIo::readTag(stream, NbtVariant::LittleEndian));
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed block entities for chunk %d %d: %s", chunkX, chunkZ,
                 exception.what());
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
        LOG_WARN(LogAreaID::Server, "Could not save weather: %s", status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::loadWeather(bool &raining, int32_t &rainTime, bool &thundering, int32_t &thunderTime) {
    if (mDb == nullptr)
        return false;

    std::string data;
    if (!mDb->Get(leveldb::ReadOptions(), WEATHER_KEY, &data).ok())
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

bool LevelStorage::saveGameRules(const Tag &rules) {
    if (mDb == nullptr)
        return false;

    BinaryStream stream;
    NbtIo::writeTag(stream, rules, NbtVariant::LittleEndian);

    const leveldb::Status status = mDb->Put(leveldb::WriteOptions(), GAME_RULES_KEY, stream.getBuffer());
    if (!status.ok()) {
        LOG_WARN(LogAreaID::Server, "Could not save game rules: %s", status.ToString().c_str());
        return false;
    }

    return true;
}

bool LevelStorage::loadGameRules(Tag &rules) {
    if (mDb == nullptr)
        return false;

    std::string data;
    if (!mDb->Get(leveldb::ReadOptions(), GAME_RULES_KEY, &data).ok())
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

void LevelStorage::writeLevelDat(const std::string &levelName, int32_t spawnX, int32_t spawnY, int32_t spawnZ,
                                 int32_t gameType, int32_t difficulty, int64_t seed, int64_t time) const {
    if (mPath.empty())
        return;

    const std::filesystem::path root = std::filesystem::path(mPath).parent_path();

    Tag data = Tag::ofCompound();
    data.putString("LevelName", levelName);
    data.putInt("SpawnX", spawnX);
    data.putInt("SpawnY", spawnY);
    data.putInt("SpawnZ", spawnZ);
    data.putInt("GameType", gameType);
    data.putInt("Difficulty", difficulty);
    data.putLong("RandomSeed", seed);
    data.putLong("Time", time);
    data.putInt("StorageVersion", 10);
    data.putInt("NetworkVersion", 2193);
    data.putByte("commandsEnabled", 1);
    data.putInt("limitedWorldOriginX", spawnX);
    data.putInt("limitedWorldOriginY", spawnY);
    data.putInt("limitedWorldOriginZ", spawnZ);
    data.putLong("LastPlayed", 0);

    BinaryStream body;
    NbtIo::writeTag(body, data, NbtVariant::LittleEndian);

    const std::string payload = body.getBuffer();

    BinaryStream file;
    file.putLInt(10);
    file.putLInt((uint32_t) payload.size());
    file.put(payload);

    std::ofstream out((root / "level.dat").string(), std::ios::binary);
    if (!out.is_open()) {
        LOG_WARN(LogAreaID::Server, "Could not write level.dat for %s", levelName.c_str());
        return;
    }

    const std::string bytes = file.getBuffer();
    out.write(bytes.data(), (std::streamsize) bytes.size());
}

bool LevelStorage::readLevelDatLong(const std::string &key, int64_t &value) const {
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
        const Tag data = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        if (!data.contains(key))
            return false;

        value = data.getLong(key);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Malformed level.dat: %s", exception.what());
        return false;
    }

    return true;
}
