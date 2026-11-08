#include "World/Level/Map/ImageMap.h"

#include <climits>

// png only, from memory, refused past MAX_IMAGE_SIDE before any pixel is allocated
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 4096
#include <stb_image.h>

static_assert(STBI_MAX_DIMENSIONS == ImageMap::MAX_IMAGE_SIDE, "the decoder limit follows MAX_IMAGE_SIDE");

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
