#pragma once


#include <cstddef>
#include <cstdint>

namespace pixelops {

size_t rgbaSize(int width, int height);

void reconstructAlpha(uint8_t* rgba, uint8_t const* coverage, int pixelCount);

void unpremultiply(uint8_t* rgba, int pixelCount);

void alphaToGrayscale(uint8_t* rgba, int pixelCount);

bool visibleBounds(uint8_t const* rgba, int width, int height, int& outX, int& outY, int& outW, int& outH);

bool crop(uint8_t const* src, int srcWidth, int srcHeight, int cropX, int cropY, int cropW, int cropH,
          uint8_t* dst);

void rgbaToRgb(uint8_t const* rgba, int pixelCount, uint8_t* rgb);

constexpr int c_gifPaletteSize = 256 * 3;

void writeGifPalette(uint8_t* palette);

void gifPaletteEntry(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b);

bool quantizeToGif(uint8_t const* rgba, int width, int height, bool transparentBg, uint8_t* indices);

}
