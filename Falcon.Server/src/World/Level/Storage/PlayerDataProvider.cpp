#include "World/Level/Storage/PlayerDataProvider.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Text/StringUtil.h"
#include "Core/Utility/BinaryStream.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <zlib.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIRECTORY(path) _mkdir(path)
#else
#define MAKE_DIRECTORY(path) mkdir(path, 0755)
#endif

namespace {
    const int GZIP_WINDOW_BITS = 15 + 16;
    const int GZIP_MEMORY_LEVEL = 8;
    const size_t DECOMPRESS_CHUNK_SIZE = 32768;
}

PlayerDataProvider::PlayerDataProvider(const std::string &directory) : mDirectory(directory) {
    MAKE_DIRECTORY(mDirectory.c_str());
}

bool PlayerDataProvider::isSafeName(const std::string &name) {
    // client-supplied name: no escaping the directory, no path injection; control bytes rejected (a NUL
    // truncates the path)
    if (name.empty() || name.size() > 64 || name == "." || name == "..")
        return false;

    for (const unsigned char character: name) {
        // ':' opens an alternate stream on windows
        if (character == '/' || character == '\\' || character == ':' || character < 0x20)
            return false;
    }

    if (name.find("..") != std::string::npos)
        return false;

#ifdef _WIN32
    // windows only: characters a file name cannot hold, a trailing dot or space it drops, and device
    // names, which open the device whatever the extension
    if (name.find_first_of("<>\"|?*") != std::string::npos || name.back() == '.' || name.back() == ' ')
        return false;

    const std::string stem = StringUtil::toLowerCase(name.substr(0, name.find('.')));
    static const char *const devices[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5",
                                          "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4",
                                          "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
    for (const char *device: devices) {
        if (stem == device)
            return false;
    }
#endif

    return true;
}

std::string PlayerDataProvider::_getPlayerDataPath(const std::string &name) const {
    return mDirectory + "/" + StringUtil::toLowerCase(name) + ".dat";
}

bool PlayerDataProvider::_gzipCompress(const std::string &input, std::string &output) {
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));

    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, GZIP_WINDOW_BITS, GZIP_MEMORY_LEVEL,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    output.resize(deflateBound(&stream, (uLong) input.size()) + 32);

    stream.next_in = (Bytef *) input.data();
    stream.avail_in = (uInt) input.size();
    stream.next_out = (Bytef *) &output[0];
    stream.avail_out = (uInt) output.size();

    const int result = deflate(&stream, Z_FINISH);
    const size_t written = output.size() - stream.avail_out;
    deflateEnd(&stream);

    if (result != Z_STREAM_END)
        return false;

    output.resize(written);
    return true;
}

bool PlayerDataProvider::_gzipDecompress(const std::string &input, std::string &output) {
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

bool PlayerDataProvider::hasData(const std::string &name) const {
    if (!isSafeName(name))
        return false;

    struct stat info;
    return stat(_getPlayerDataPath(name).c_str(), &info) == 0;
}

bool PlayerDataProvider::loadData(const std::string &name, Tag &outData) const {
    if (!isSafeName(name))
        return false;

    std::ifstream file(_getPlayerDataPath(name), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream contents;
    contents << file.rdbuf();
    file.close();

    std::string decompressed;
    if (!_gzipDecompress(contents.str(), decompressed))
        return false;

    try {
        ReadOnlyBinaryStream stream(decompressed);
        outData = NbtIo::readTag(stream, NbtVariant::BigEndian);
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

std::string PlayerDataProvider::_getArchivePath(const std::string &name, const std::string &xuid) const {
    return mDirectory + "/" + StringUtil::toLowerCase(name) + "." + xuid + ".dat";
}

bool PlayerDataProvider::claimData(const std::string &name, const std::string &xuid) const {
    if (xuid.empty() || !isSafeName(name) || !isSafeName(xuid))
        return true;

    const std::string live = _getPlayerDataPath(name);
    const std::string mine = _getArchivePath(name, xuid);

    Tag data;
    if (loadData(name, data)) {
        const std::string owner = data.getString(TAG_OWNER_XUID, std::string());
        if (owner.empty() || owner == xuid)
            return true;
        if (!isSafeName(owner))
            return false;

        // rename does not replace on windows: an older archive of that account goes first, the live
        // save being the newer of the two
        const std::string theirs = _getArchivePath(name, owner);
        std::remove(theirs.c_str());
        if (std::rename(live.c_str(), theirs.c_str()) != 0)
            return false;
    }

    struct stat info;
    if (stat(mine.c_str(), &info) != 0)
        return true;
    return std::rename(mine.c_str(), live.c_str()) == 0;
}

bool PlayerDataProvider::saveData(const std::string &name, const Tag &data) const {
    if (!isSafeName(name))
        return false;

    BinaryStream stream;

    try {
        NbtIo::writeTag(stream, data, NbtVariant::BigEndian);
    } catch (const std::exception &) {
        return false;
    }

    std::string compressed;
    if (!_gzipCompress(stream.getBuffer(), compressed))
        return false;

    std::ofstream file(_getPlayerDataPath(name), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;

    file.write(compressed.data(), (std::streamsize) compressed.size());
    file.flush();
    return file.good();
}
