#include "Core/Archive/Gzip.h"

#include <cstring>
#include <zlib.h>

namespace {
    const int GZIP_WINDOW_BITS = 16 + MAX_WBITS;
    const size_t DECOMPRESS_CHUNK_SIZE = 65536;
}

bool Gzip::decompress(const std::string &input, std::string &output) {
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

bool Gzip::compress(const std::string &input, std::string &output) {
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, GZIP_WINDOW_BITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    output.assign(deflateBound(&stream, (uLong) input.size()), '\0');
    stream.next_in = (Bytef *) input.data();
    stream.avail_in = (uInt) input.size();
    stream.next_out = (Bytef *) &output[0];
    stream.avail_out = (uInt) output.size();
    const int result = deflate(&stream, Z_FINISH);
    output.resize(output.size() - stream.avail_out);
    deflateEnd(&stream);
    return result == Z_STREAM_END;
}
