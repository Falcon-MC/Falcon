#include "World/Level/Map/ImageMap.h"

#include <zlib.h>

#include <climits>
#include <cstring>

// png only, from memory; sides past MAX_IMAGE_SIDE are refused from the header
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 4096
#include <stb_image.h>

static_assert(STBI_MAX_DIMENSIONS == ImageMap::MAX_IMAGE_SIDE, "the decoder limit follows MAX_IMAGE_SIDE");

namespace {
    // stb tolerates trailing zeros some encoders write after the pixels
    const uint64_t INFLATE_SLACK = 64u * 1024u;

    uint32_t readBig(const uint8_t *data) {
        return ((uint32_t) data[0] << 24) | ((uint32_t) data[1] << 16) | ((uint32_t) data[2] << 8) | data[3];
    }

    uint64_t scanlineBytes(uint64_t width, uint64_t height, uint64_t bitsPerPixel) {
        if (width == 0 || height == 0)
            return 0;

        return ((width * bitsPerPixel + 7) / 8 + 1) * height;
    }

    // the filtered scanlines IHDR announces, adam7 passes included
    bool expectedInflatedSize(const uint8_t *header, uint64_t &out) {
        const uint64_t width = readBig(header);
        const uint64_t height = readBig(header + 4);
        const uint8_t depth = header[8];
        const uint8_t color = header[9];
        const uint8_t interlace = header[12];
        uint64_t channels = 0;
        switch (color) {
            case 0:
            case 3:
                channels = 1;
                break;
            case 2:
                channels = 3;
                break;
            case 4:
                channels = 2;
                break;
            case 6:
                channels = 4;
                break;
            default:
                return false;
        }
        if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16)
            return false;

        const uint64_t bitsPerPixel = channels * depth;
        if (interlace == 0) {
            out = scanlineBytes(width, height, bitsPerPixel);
            return true;
        }

        const uint64_t passes[7][4] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4},
                                       {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
        out = 0;
        for (const uint64_t *pass: passes) {
            const uint64_t columns = width > pass[0] ? (width - pass[0] + pass[2] - 1) / pass[2] : 0;
            const uint64_t rows = height > pass[1] ? (height - pass[1] + pass[3] - 1) / pass[3] : 0;
            out += scanlineBytes(columns, rows, bitsPerPixel);
        }
        return true;
    }

    // stb grows its inflate buffer up to 2 GB before it compares sizes: inflate once into a scratch buffer first
    bool inflatesWithinHeader(const uint8_t *data, size_t length, std::string &error) {
        error = "not a png file";
        size_t offset = 8;
        uint64_t limit = 0;
        bool header = false;
        // CgBI files hold a raw deflate stream, the others a zlib one whose 2 byte header stb checks
        size_t skip = 2;

        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            return false;

        uint8_t scratch[32 * 1024];
        bool ok = false;
        while (offset + 12 <= length) {
            const uint32_t size = readBig(data + offset);
            const uint8_t *type = data + offset + 4;
            const uint8_t *body = data + offset + 8;
            if (size > length - offset - 12)
                break;
            offset += 12 + (size_t) size;

            if (std::memcmp(type, "CgBI", 4) == 0) {
                skip = 0;
                continue;
            }
            if (std::memcmp(type, "IHDR", 4) == 0) {
                if (header || size != 13 || !expectedInflatedSize(body, limit))
                    break;
                header = true;
                limit += INFLATE_SLACK;
                continue;
            }
            if (std::memcmp(type, "IEND", 4) == 0) {
                ok = header;
                break;
            }
            if (std::memcmp(type, "IDAT", 4) != 0)
                continue;
            if (!header)
                break;

            const size_t skipped = skip < size ? skip : size;
            skip -= skipped;
            stream.next_in = const_cast<Bytef *>(body + skipped);
            stream.avail_in = (uInt) (size - skipped);
            int status = Z_OK;
            while (stream.avail_in > 0 && status == Z_OK) {
                stream.next_out = scratch;
                stream.avail_out = sizeof(scratch);
                status = inflate(&stream, Z_NO_FLUSH);
                if (stream.total_out > limit) {
                    error = "the png holds more pixel data than its header announces";
                    inflateEnd(&stream);
                    return false;
                }
            }
            if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
                error = "broken png: bad compressed data";
                break;
            }
        }
        inflateEnd(&stream);
        return ok;
    }
}

bool ImageMap::decodePng(const uint8_t *data, size_t length, ImageMapImage &out, std::string &error) {
    if (data == nullptr || length == 0 || length > MAX_PNG_SIZE || length > (size_t) INT_MAX) {
        error = "not a png file";
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(data, (int) length, &width, &height, &channels) == 0) {
        error = "not a png file";
        return false;
    }
    if (width <= 0 || height <= 0 || width > MAX_IMAGE_SIDE || height > MAX_IMAGE_SIDE) {
        error = "images go up to " + std::to_string(MAX_IMAGE_SIDE) + " pixels a side";
        return false;
    }

    if (!inflatesWithinHeader(data, length, error))
        return false;

    stbi_uc *pixels = stbi_load_from_memory(data, (int) length, &width, &height, &channels, 4);
    if (pixels == nullptr) {
        error = std::string("broken png: ") + stbi_failure_reason();
        return false;
    }

    const size_t bytes = (size_t) width * (size_t) height * 4;
    const bool loaded = fromRgba(pixels, bytes, width, height, out, error);
    stbi_image_free(pixels);
    return loaded;
}
