#include "World/Level/Map/ImageMap.h"

#include <algorithm>
#include <cmath>

namespace {
    struct Tap {
        int32_t mIndex;
        float mWeight;
    };

    // area average: each target pixel takes the source pixels its span covers, an upscale repeats them
    std::vector<std::vector<Tap>> taps(double start, double length, int32_t sourceSize, int32_t targetSize) {
        std::vector<std::vector<Tap>> result((size_t) targetSize);
        const double step = length / (double) targetSize;
        for (int32_t target = 0; target < targetSize; ++target) {
            const double from = start + step * target;
            const double to = from + step;
            double total = 0.0;
            std::vector<Tap> &entry = result[(size_t) target];
            for (int32_t index = (int32_t) std::floor(from); (double) index < to; ++index) {
                const double weight = std::min(to, (double) index + 1.0) - std::max(from, (double) index);
                if (weight <= 0.0)
                    continue;

                entry.push_back({std::min(std::max(index, 0), sourceSize - 1), (float) weight});
                total += weight;
            }
            for (Tap &tap: entry)
                tap.mWeight = (float) (tap.mWeight / total);
        }
        return result;
    }

    void resample(const ImageMapImage &source, double sourceX, double sourceY, double sourceWidth,
                  double sourceHeight, ImageMapImage &target, int32_t targetX, int32_t targetY, int32_t targetWidth,
                  int32_t targetHeight) {
        const std::vector<std::vector<Tap>> columns = taps(sourceX, sourceWidth, source.mWidth, targetWidth);
        const std::vector<std::vector<Tap>> rows = taps(sourceY, sourceHeight, source.mHeight, targetHeight);

        for (int32_t y = 0; y < targetHeight; ++y) {
            for (int32_t x = 0; x < targetWidth; ++x) {
                // premultiplied: a transparent neighbour must not darken the edge
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                float alpha = 0.0f;
                for (const Tap &row: rows[(size_t) y]) {
                    for (const Tap &column: columns[(size_t) x]) {
                        const uint32_t pixel = source.mPixels[(size_t) column.mIndex
                                                              + (size_t) row.mIndex * (size_t) source.mWidth];
                        const float weight = row.mWeight * column.mWeight;
                        const float pixelAlpha = (float) (pixel >> 24) * weight;
                        red += (float) (pixel & 0xff) * pixelAlpha;
                        green += (float) ((pixel >> 8) & 0xff) * pixelAlpha;
                        blue += (float) ((pixel >> 16) & 0xff) * pixelAlpha;
                        alpha += pixelAlpha;
                    }
                }

                uint32_t packed = 0;
                if (alpha > 0.0f) {
                    const auto channel = [](float value) {
                        return (uint8_t) std::min(255.0f, std::max(0.0f, std::round(value)));
                    };
                    packed = ImageMap::pack(channel(red / alpha), channel(green / alpha), channel(blue / alpha),
                                            channel(alpha));
                }
                target.mPixels[(size_t) (targetX + x) + (size_t) (targetY + y) * (size_t) target.mWidth] = packed;
            }
        }
    }
}

uint32_t ImageMap::pack(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    return (uint32_t) red | ((uint32_t) green << 8) | ((uint32_t) blue << 16) | ((uint32_t) alpha << 24);
}

bool ImageMap::fromRgba(const uint8_t *rgba, size_t length, int32_t width, int32_t height, ImageMapImage &out,
                        std::string &error) {
    if (width <= 0 || height <= 0 || width > MAX_IMAGE_SIDE || height > MAX_IMAGE_SIDE) {
        error = "images go up to " + std::to_string(MAX_IMAGE_SIDE) + " pixels a side";
        return false;
    }

    const size_t count = (size_t) width * (size_t) height;
    if (rgba == nullptr || length != count * 4) {
        error = "expected " + std::to_string(count * 4) + " bytes of rgba";
        return false;
    }

    out.mWidth = width;
    out.mHeight = height;
    out.mPixels.resize(count);
    for (size_t index = 0; index < count; ++index) {
        const uint8_t *pixel = rgba + index * 4;
        out.mPixels[index] = pack(pixel[0], pixel[1], pixel[2], pixel[3]);
    }
    return true;
}

bool ImageMap::gridSize(int32_t width, int32_t height, int32_t &columns, int32_t &rows, std::string &error) {
    if (width <= 0 || height <= 0 || columns < 0 || rows < 0) {
        error = "the grid needs a positive size";
        return false;
    }

    if (columns == 0 && rows == 0) {
        columns = (width + TILE_SIZE - 1) / TILE_SIZE;
        rows = (height + TILE_SIZE - 1) / TILE_SIZE;
    } else if (rows == 0) {
        rows = std::max(1, (int32_t) std::lround((double) columns * height / width));
    } else if (columns == 0) {
        columns = std::max(1, (int32_t) std::lround((double) rows * width / height));
    }

    if (columns > MAX_GRID_SIDE || rows > MAX_GRID_SIDE) {
        error = "image maps go up to " + std::to_string(MAX_GRID_SIDE) + " x " + std::to_string(MAX_GRID_SIDE)
                + " maps";
        return false;
    }
    return true;
}

ImageMapImage ImageMap::fitTo(const ImageMapImage &image, int32_t width, int32_t height, ImageMapFit fit) {
    ImageMapImage result;
    result.mWidth = width;
    result.mHeight = height;
    result.mPixels.assign((size_t) width * (size_t) height, 0);
    if (image.mWidth <= 0 || image.mHeight <= 0 || width <= 0 || height <= 0)
        return result;

    if (image.mWidth == width && image.mHeight == height) {
        result.mPixels = image.mPixels;
        return result;
    }

    const double scaleX = (double) width / image.mWidth;
    const double scaleY = (double) height / image.mHeight;
    if (fit == ImageMapFit::Contain) {
        const double scale = std::min(scaleX, scaleY);
        const int32_t fittedWidth = std::min(width, std::max(1, (int32_t) std::lround(image.mWidth * scale)));
        const int32_t fittedHeight = std::min(height, std::max(1, (int32_t) std::lround(image.mHeight * scale)));
        resample(image, 0.0, 0.0, image.mWidth, image.mHeight, result, (width - fittedWidth) / 2,
                 (height - fittedHeight) / 2, fittedWidth, fittedHeight);
        return result;
    }

    if (fit == ImageMapFit::Cover) {
        const double scale = std::max(scaleX, scaleY);
        const double sourceWidth = width / scale;
        const double sourceHeight = height / scale;
        resample(image, (image.mWidth - sourceWidth) / 2.0, (image.mHeight - sourceHeight) / 2.0, sourceWidth,
                 sourceHeight, result, 0, 0, width, height);
        return result;
    }

    resample(image, 0.0, 0.0, image.mWidth, image.mHeight, result, 0, 0, width, height);
    return result;
}

std::vector<ImageMapTile> ImageMap::split(const ImageMapImage &image, int32_t columns, int32_t rows) {
    std::vector<ImageMapTile> tiles;
    if (columns <= 0 || rows <= 0 || image.mWidth != columns * TILE_SIZE || image.mHeight != rows * TILE_SIZE)
        return tiles;

    tiles.reserve((size_t) (columns * rows));
    for (int32_t row = 0; row < rows; ++row) {
        for (int32_t column = 0; column < columns; ++column) {
            ImageMapTile tile((size_t) (TILE_SIZE * TILE_SIZE));
            for (int32_t y = 0; y < TILE_SIZE; ++y) {
                const auto first = image.mPixels.begin() + (ptrdiff_t) (column * TILE_SIZE)
                                   + (ptrdiff_t) (row * TILE_SIZE + y) * image.mWidth;
                std::copy(first, first + TILE_SIZE, tile.begin() + (ptrdiff_t) (y * TILE_SIZE));
            }
            tiles.push_back(std::move(tile));
        }
    }
    return tiles;
}

bool ImageMap::cut(const ImageMapImage &image, int32_t &columns, int32_t &rows, ImageMapFit fit,
                   std::vector<ImageMapTile> &tiles, std::string &error) {
    if (!gridSize(image.mWidth, image.mHeight, columns, rows, error))
        return false;

    tiles = split(fitTo(image, columns * TILE_SIZE, rows * TILE_SIZE, fit), columns, rows);
    return true;
}

Vector3i ImageMap::rightOf(int32_t facing) {
    switch (facing) {
        case 2:
            return Vector3i(-1, 0, 0);
        case 3:
            return Vector3i(1, 0, 0);
        case 4:
            return Vector3i(0, 0, 1);
        case 5:
            return Vector3i(0, 0, -1);
        default:
            return Vector3i(0, 0, 0);
    }
}

Vector3i ImageMap::supportOf(const Vector3i &position, int32_t facing) {
    switch (facing) {
        case 2:
            return Vector3i(position.x, position.y, position.z + 1);
        case 3:
            return Vector3i(position.x, position.y, position.z - 1);
        case 4:
            return Vector3i(position.x + 1, position.y, position.z);
        case 5:
            return Vector3i(position.x - 1, position.y, position.z);
        default:
            return position;
    }
}

bool ImageMap::wallPositions(const Vector3i &topLeft, int32_t facing, int32_t columns, int32_t rows,
                             std::vector<Vector3i> &out) {
    out.clear();
    if (facing < 2 || facing > 5 || columns <= 0 || rows <= 0 || columns > MAX_GRID_SIDE || rows > MAX_GRID_SIDE)
        return false;

    const Vector3i right = rightOf(facing);
    out.reserve((size_t) (columns * rows));
    for (int32_t row = 0; row < rows; ++row) {
        for (int32_t column = 0; column < columns; ++column)
            out.emplace_back(topLeft.x + right.x * column, topLeft.y - row, topLeft.z + right.z * column);
    }
    return true;
}
