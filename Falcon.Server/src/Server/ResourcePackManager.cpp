#include "Server/ResourcePackManager.h"

#include "Core/Archive/ZipArchive.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/Pack/PackDependencies.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <zlib.h>
#include <openssl/sha.h>

namespace {

    uint16_t readU16(const std::string &data, size_t offset) {
        return (uint16_t) ((unsigned char) data[offset] | ((unsigned char) data[offset + 1] << 8));
    }

    uint32_t readU32(const std::string &data, size_t offset) {
        return (uint32_t) ((unsigned char) data[offset] | ((unsigned char) data[offset + 1] << 8) |
                           ((unsigned char) data[offset + 2] << 16) | ((unsigned char) data[offset + 3] << 24));
    }

    bool inflateRaw(const std::string &input, size_t uncompressedSize, std::string &out) {
        out.resize(uncompressedSize);

        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));

        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            return false;

        stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
        stream.avail_in = (uInt) input.size();
        stream.next_out = reinterpret_cast<Bytef *>(&out[0]);
        stream.avail_out = (uInt) uncompressedSize;

        const int result = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);

        return result == Z_STREAM_END;
    }

    std::string findJsonString(const std::string &json, const std::string &key) {
        const std::string pattern = "\"" + key + "\"";

        size_t position = json.find(pattern);
        if (position == std::string::npos)
            return std::string();

        position = json.find(':', position + pattern.size());
        if (position == std::string::npos)
            return std::string();

        position = json.find('"', position);
        if (position == std::string::npos)
            return std::string();

        std::string value;
        for (size_t i = position + 1; i < json.size(); i++) {
            if (json[i] == '"')
                return value;
            value.push_back(json[i]);
        }

        return std::string();
    }

    std::string extractObject(const std::string &json, const std::string &key) {
        const std::string pattern = "\"" + key + "\"";
        size_t position = json.find(pattern);
        if (position == std::string::npos)
            return std::string();

        position = json.find('{', position + pattern.size());
        if (position == std::string::npos)
            return std::string();

        int depth = 0;
        for (size_t i = position; i < json.size(); i++) {
            if (json[i] == '{')
                depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0)
                    return json.substr(position, i - position + 1);
            }
        }

        return std::string();
    }

    std::string extractVersion(const std::string &header) {
        const std::string pattern = "\"version\"";
        size_t position = header.find(pattern);
        if (position == std::string::npos)
            return "1.0.0";

        position = header.find('[', position + pattern.size());
        if (position == std::string::npos)
            return "1.0.0";

        const size_t end = header.find(']', position);
        if (end == std::string::npos)
            return "1.0.0";

        std::string numbers;
        std::string current;
        for (size_t i = position + 1; i < end; i++) {
            const char c = header[i];
            if (c >= '0' && c <= '9') {
                current.push_back(c);
            } else if (!current.empty()) {
                numbers += (numbers.empty() ? "" : ".") + current;
                current.clear();
            }
        }
        if (!current.empty())
            numbers += (numbers.empty() ? "" : ".") + current;

        return numbers.empty() ? "1.0.0" : numbers;
    }

}

bool ResourcePackManager::_readWholeFile(const std::string &path, std::string &out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    const std::streamsize size = file.tellg();
    if (size <= 0)
        return false;

    out.resize((size_t) size);
    file.seekg(0);
    file.read(&out[0], size);
    return file.good();
}

bool ResourcePackManager::_extractZipEntry(const std::string &zip, const std::string &suffix, std::string &out) {
    if (zip.size() < 22)
        return false;

    size_t eocd = std::string::npos;
    for (size_t i = zip.size() - 22; i + 4 <= zip.size(); i--) {
        if (readU32(zip, i) == 0x06054b50) {
            eocd = i;
            break;
        }
        if (i == 0)
            break;
    }

    if (eocd == std::string::npos)
        return false;

    const uint16_t entryCount = readU16(zip, eocd + 10);
    uint32_t offset = readU32(zip, eocd + 16);

    for (uint16_t entry = 0; entry < entryCount; entry++) {
        if (offset + 46 > zip.size() || readU32(zip, offset) != 0x02014b50)
            return false;

        const uint16_t method = readU16(zip, offset + 10);
        const uint32_t compressedSize = readU32(zip, offset + 20);
        const uint32_t uncompressedSize = readU32(zip, offset + 24);
        const uint16_t nameLength = readU16(zip, offset + 28);
        const uint16_t extraLength = readU16(zip, offset + 30);
        const uint16_t commentLength = readU16(zip, offset + 32);
        const uint32_t localOffset = readU32(zip, offset + 42);

        const std::string name = zip.substr(offset + 46, nameLength);
        offset += 46 + nameLength + extraLength + commentLength;

        const bool matches = name.size() >= suffix.size() &&
                             name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
        if (!matches)
            continue;

        if (localOffset + 30 > zip.size() || readU32(zip, localOffset) != 0x04034b50)
            return false;

        const uint16_t localNameLength = readU16(zip, localOffset + 26);
        const uint16_t localExtraLength = readU16(zip, localOffset + 28);
        const size_t dataStart = localOffset + 30 + localNameLength + localExtraLength;

        if (dataStart + compressedSize > zip.size())
            return false;

        const std::string raw = zip.substr(dataStart, compressedSize);

        if (method == 0) {
            out = raw;
            return true;
        }

        if (method == 8)
            return inflateRaw(raw, uncompressedSize, out);

        return false;
    }

    return false;
}

bool ResourcePackManager::_readManifest(const std::string &manifest, std::string &uuid, std::string &version,
                                        std::string &name) {
    const std::string header = extractObject(manifest, "header");
    if (header.empty())
        return false;

    uuid = findJsonString(header, "uuid");
    version = extractVersion(header);
    name = findJsonString(header, "name");
    return !uuid.empty();
}

std::string ResourcePackManager::_sha256(const std::string &data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(data.data()), data.size(), digest);
    return std::string(reinterpret_cast<const char *>(digest), SHA256_DIGEST_LENGTH);
}

bool ResourcePackManager::_loadPackBytes(std::string data, const std::string &label, const std::string &contentKey) {
    std::string manifest;
    if (!_extractZipEntry(data, "manifest.json", manifest)) {
        LOG_WARN(LogAreaID::Server, "Resource pack %s has no readable manifest.json", label.c_str());
        return false;
    }

    std::string uuid;
    std::string version;
    std::string name;
    if (!_readManifest(manifest, uuid, version, name)) {
        LOG_WARN(LogAreaID::Server, "Resource pack %s manifest is missing a header uuid", label.c_str());
        return false;
    }

    for (const ResourcePack &existing: mPacks) {
        if (existing.mUuidString == uuid) {
            LOG_WARN(LogAreaID::Server, "Skipping duplicate resource pack %s (uuid %s already loaded)",
                     label.c_str(), uuid.c_str());
            return false;
        }
    }

    ResourcePack pack;
    pack.mDependencyUuids = collectPackDependencyUuids(manifest);
    pack.mUuidString = uuid;
    pack.mUuid = Uuid::fromString(uuid);
    pack.mVersion = version;
    pack.mName = name.empty() ? label : name;
    pack.mData = std::move(data);
    pack.mSize = pack.mData.size();
    pack.mSha256 = _sha256(pack.mData);
    pack.mContentKey = contentKey;

    if (!contentKey.empty())
        LOG_INFO(LogAreaID::Server, "Resource pack %s is encrypted (loaded content key)", pack.mName.c_str());

    mPacks.push_back(std::move(pack));
    return true;
}

bool ResourcePackManager::_loadPackFile(const std::string &path, const std::string &fileName) {
    std::string data;
    if (!_readWholeFile(path, data)) {
        LOG_WARN(LogAreaID::Server, "Could not read resource pack %s", fileName.c_str());
        return false;
    }

    std::filesystem::path keyPath = std::filesystem::path(path);
    keyPath.replace_extension(".key");

    std::string key;
    if (_readWholeFile(keyPath.string(), key)) {
        while (!key.empty() && (key.back() == '\n' || key.back() == '\r' || key.back() == ' ' || key.back() == '\t'))
            key.pop_back();
    } else {
        key.clear();
    }

    return _loadPackBytes(std::move(data), fileName, key);
}

void ResourcePackManager::_loadFolder(const std::string &path, const std::string &label) {
    std::error_code error;
    if (!std::filesystem::exists(std::filesystem::path(path) / "manifest.json", error))
        return;

    ZipWriter writer;
    const std::filesystem::path base(path);

    for (const std::filesystem::directory_entry &entry:
                 std::filesystem::recursive_directory_iterator(base, error)) {
        if (!entry.is_regular_file())
            continue;

        std::string content;
        if (!_readWholeFile(entry.path().string(), content))
            continue;

        const std::string relative = std::filesystem::relative(entry.path(), base, error).generic_string();
        writer.addFile(relative, content);
    }

    _loadPackBytes(writer.finish(), label, std::string());
}

void ResourcePackManager::loadBundledAddonsFrom(const std::string &directory) {
    std::error_code error;
    const std::filesystem::path root(directory);
    if (!std::filesystem::exists(root, error))
        return;

    for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(root, error)) {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension().string() != ".mcaddon")
            continue;

        std::string data;
        if (_readWholeFile(entry.path().string(), data))
            _loadAddon(data);
    }
}

void ResourcePackManager::_loadAddon(const std::string &data) {
    std::vector<ZipEntry> entries;
    if (!ZipArchive::listEntries(data, entries))
        return;

    std::vector<std::string> packRoots;
    for (const ZipEntry &entry: entries) {
        const std::string suffix = "manifest.json";
        if (entry.mName.size() < suffix.size()
            || entry.mName.compare(entry.mName.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;

        const size_t slash = entry.mName.find_last_of('/');
        packRoots.push_back(slash == std::string::npos ? std::string() : entry.mName.substr(0, slash + 1));
    }

    for (const std::string &packRoot: packRoots) {
        std::string manifest;
        for (const ZipEntry &entry: entries) {
            if (entry.mName == packRoot + "manifest.json") {
                manifest = entry.mData;
                break;
            }
        }

        if (manifest.find("\"resources\"") == std::string::npos)
            continue;

        ZipWriter writer;
        for (const ZipEntry &entry: entries) {
            if (packRoot.empty() || entry.mName.compare(0, packRoot.size(), packRoot) == 0)
                writer.addFile(entry.mName.substr(packRoot.size()), entry.mData);
        }

        _loadPackBytes(writer.finish(), "addon:" + packRoot, std::string());
    }
}

void ResourcePackManager::loadFromDirectory(const std::string &directory) {
    mPacks.clear();

    std::error_code error;
    if (!std::filesystem::exists(directory, error)) {
        std::filesystem::create_directories(directory, error);
        return;
    }

    for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_directory()) {
            _loadFolder(entry.path().string(), entry.path().filename().string());
            continue;
        }

        if (!entry.is_regular_file())
            continue;

        const std::string extension = entry.path().extension().string();

        if (extension == ".zip" || extension == ".mcpack") {
            _loadPackFile(entry.path().string(), entry.path().filename().string());
        } else if (extension == ".mcaddon") {
            std::string data;
            if (_readWholeFile(entry.path().string(), data))
                _loadAddon(data);
        }
    }

    LOG_TRACE(LogAreaID::Server, "Resource pack manager loaded %zu pack(s)", mPacks.size());
}

bool ResourcePackManager::loadCdnConfig(const std::string &path) {
    std::string source;
    if (!_readWholeFile(path, source))
        return false;

    const std::unique_ptr<json::Value> root = json::parse(source);
    const json::Value *entries = root == nullptr ? nullptr : root->get("resource_packs");
    if (entries == nullptr || !entries->isArray()) {
        LOG_ERROR(LogAreaID::Server, "Failed to parse CDN config");
        return false;
    }

    for (const std::unique_ptr<json::Value> &entry: entries->mArray) {
        if (!entry->isObject())
            continue;

        const json::Value *packId = entry->get("pack_id");
        const json::Value *url = entry->get("url");
        if (packId == nullptr || url == nullptr || !packId->isString() || !url->isString())
            continue;

        const json::Value *version = entry->get("version");
        const std::string wantedVersion = version == nullptr ? std::string() : version->string();

        auto pack = std::find_if(mPacks.begin(), mPacks.end(), [&](const ResourcePack &candidate) {
            return candidate.mUuidString == packId->mString
                   && (wantedVersion.empty() || candidate.mVersion == wantedVersion);
        });

        if (pack == mPacks.end()) {
            LOG_WARN(LogAreaID::Server, "Mismatch detected between CDN config and required resource packs: %s",
                     packId->mString.c_str());
            continue;
        }

        pack->mCdnUrl = url->mString;
        LOG_INFO(LogAreaID::Server, "CDN URL for resource pack %s: %s", pack->mName.c_str(),
                 pack->mCdnUrl.c_str());
    }

    return true;
}

const ResourcePack *ResourcePackManager::findById(const std::string &uuid) const {
    for (const ResourcePack &pack: mPacks) {
        if (pack.mUuidString == uuid)
            return &pack;
    }
    return nullptr;
}

std::vector<std::string> ResourcePackManager::getLoadedUuids() const {
    std::vector<std::string> uuids;
    uuids.reserve(mPacks.size());
    for (const ResourcePack &pack: mPacks)
        uuids.push_back(pack.mUuidString);
    return uuids;
}

size_t ResourcePackManager::pruneUnsatisfied(const std::vector<std::string> &availableUuids) {
    size_t removed = 0;

    for (size_t i = 0; i < mPacks.size();) {
        bool satisfied = true;
        for (const std::string &dependency: mPacks[i].mDependencyUuids) {
            if (std::find(availableUuids.begin(), availableUuids.end(), dependency) == availableUuids.end()) {
                satisfied = false;
                break;
            }
        }

        if (satisfied) {
            ++i;
            continue;
        }

        mPacks.erase(mPacks.begin() + (long) i);
        removed++;
    }

    return removed;
}
