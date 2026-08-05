#include "image/PixelOps.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace pixelops {

namespace {

constexpr uint8_t c_bayer8x8[64] = {
    0,  32, 8,  40, 2,  34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44, 4,  36, 14, 46, 6,  38,
    60, 28, 52, 20, 62, 30, 54, 22,
    3,  35, 11, 43, 1,  33, 9,  41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47, 7,  39, 13, 45, 5,  37,
    63, 31, 55, 23, 61, 29, 53, 21,
};

constexpr int c_cubeSteps = 6;
constexpr int c_cubeStride = 51;

constexpr int cubeChannel(uint8_t value) {
    return (value * (c_cubeSteps - 1)) / 255;
}

}

size_t rgbaSize(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    auto w = static_cast<size_t>(width);
    auto h = static_cast<size_t>(height);
    if (w > std::numeric_limits<size_t>::max() / h) return 0;
    size_t pixels = w * h;
    if (pixels > std::numeric_limits<size_t>::max() / 4) return 0;
    return pixels * 4;
}

void reconstructAlpha(uint8_t* rgba, uint8_t const* coverage, int pixelCount) {
    if (!rgba || !coverage) return;
    for (int p = 0; p < pixelCount; ++p) {
        uint8_t const* px = rgba + static_cast<size_t>(p) * 4;
        uint8_t luminance = std::max({px[0], px[1], px[2]});
        uint8_t covered = coverage[static_cast<size_t>(p) * 4 + 3];
        rgba[static_cast<size_t>(p) * 4 + 3] = std::max(covered, luminance);
    }
}

void unpremultiply(uint8_t* rgba, int pixelCount) {
    if (!rgba) return;
    for (int p = 0; p < pixelCount; ++p) {
        uint8_t* px = rgba + static_cast<size_t>(p) * 4;
        uint8_t a = px[3];
        if (a == 0) {
            px[0] = px[1] = px[2] = 0;
            continue;
        }
        if (a == 255) continue;
        for (int c = 0; c < 3; ++c) {
            int value = (px[c] * 255 + a / 2) / a;
            px[c] = static_cast<uint8_t>(std::min(value, 255));
        }
    }
}

void alphaToGrayscale(uint8_t* rgba, int pixelCount) {
    if (!rgba) return;
    for (int p = 0; p < pixelCount; ++p) {
        uint8_t* px = rgba + static_cast<size_t>(p) * 4;
        px[0] = px[1] = px[2] = px[3];
        px[3] = 255;
    }
}

bool visibleBounds(uint8_t const* rgba, int width, int height, int& outX, int& outY, int& outW, int& outH) {
    if (!rgba || width <= 0 || height <= 0) return false;

    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < height; ++y) {
        uint8_t const* row = rgba + static_cast<size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            if (!row[static_cast<size_t>(x) * 4 + 3]) continue;
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }

    if (maxX < 0) return false;

    outX = minX;
    outY = minY;
    outW = maxX - minX + 1;
    outH = maxY - minY + 1;
    return true;
}

bool crop(uint8_t const* src, int srcWidth, int srcHeight, int cropX, int cropY, int cropW, int cropH,
          uint8_t* dst) {
    if (!src || !dst) return false;
    if (cropW <= 0 || cropH <= 0) return false;
    if (cropX < 0 || cropY < 0) return false;
    if (cropX + cropW > srcWidth || cropY + cropH > srcHeight) return false;

    size_t rowBytes = static_cast<size_t>(cropW) * 4;
    for (int y = 0; y < cropH; ++y) {
        uint8_t const* srcRow = src + (static_cast<size_t>(y + cropY) * srcWidth + cropX) * 4;
        std::memcpy(dst + static_cast<size_t>(y) * rowBytes, srcRow, rowBytes);
    }
    return true;
}

void rgbaToRgb(uint8_t const* rgba, int pixelCount, uint8_t* rgb) {
    if (!rgba || !rgb) return;
    for (int p = 0; p < pixelCount; ++p) {
        uint8_t const* src = rgba + static_cast<size_t>(p) * 4;
        uint8_t* dst = rgb + static_cast<size_t>(p) * 3;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }
}

void writeGifPalette(uint8_t* palette) {
    if (!palette) return;
    std::memset(palette, 0, c_gifPaletteSize);

    int index = 1;
    for (int r = 0; r < c_cubeSteps; ++r) {
        for (int g = 0; g < c_cubeSteps; ++g) {
            for (int b = 0; b < c_cubeSteps; ++b) {
                if (index >= 256) return;
                palette[index * 3 + 0] = static_cast<uint8_t>(r * c_cubeStride);
                palette[index * 3 + 1] = static_cast<uint8_t>(g * c_cubeStride);
                palette[index * 3 + 2] = static_cast<uint8_t>(b * c_cubeStride);
                ++index;
            }
        }
    }
}

void gifPaletteEntry(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (index == 0 || index > c_cubeSteps * c_cubeSteps * c_cubeSteps) {
        r = g = b = 0;
        return;
    }
    int offset = index - 1;
    r = static_cast<uint8_t>((offset / (c_cubeSteps * c_cubeSteps)) * c_cubeStride);
    g = static_cast<uint8_t>(((offset / c_cubeSteps) % c_cubeSteps) * c_cubeStride);
    b = static_cast<uint8_t>((offset % c_cubeSteps) * c_cubeStride);
}

bool quantizeToGif(uint8_t const* rgba, int width, int height, bool transparentBg, uint8_t* indices) {
    if (!rgba || !indices || width <= 0 || height <= 0) return false;

    bool anyTransparent = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t pixel = static_cast<size_t>(y) * width + x;
            uint8_t const* px = rgba + pixel * 4;

            int threshold = (c_bayer8x8[(y & 7) * 8 + (x & 7)] * 255) / 64;
            if (transparentBg && px[3] <= threshold) {
                indices[pixel] = 0;
                anyTransparent = true;
                continue;
            }

            int r = cubeChannel(px[0]);
            int g = cubeChannel(px[1]);
            int b = cubeChannel(px[2]);
            indices[pixel] =
                static_cast<uint8_t>(1 + r * c_cubeSteps * c_cubeSteps + g * c_cubeSteps + b);
        }
    }
    return anyTransparent;
}

}
