#include "Player/PlayerDataProvider.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Text/StringUtil.h"
#include "Core/Utility/BinaryStream.h"

#include <algorithm>
#include <cctype>
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
    struct stat info;
    return stat(_getPlayerDataPath(name).c_str(), &info) == 0;
}

bool PlayerDataProvider::loadData(const std::string &name, Tag &outData) const {
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

bool PlayerDataProvider::saveData(const std::string &name, const Tag &data) const {
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
    return file.good();
}
