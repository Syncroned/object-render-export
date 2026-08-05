#include <Geode/Geode.hpp>
#include <cocos2d.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include "vendor/stb_image_write.h"
#include "image/ImageExport.hpp"
#include "image/PixelOps.hpp"
#include "utils/Paths.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace {

geode::Ref<CCImage> reloadThroughPng(CCImage* img, char const* tmpName) {
    std::filesystem::path tmpPng = Mod::get()->getSaveDir() / tmpName;
    std::string tmpPngStr = tmpPng.string();
    if (!img->saveToFile(tmpPngStr.c_str(), false)) return {};

    unsigned long len = 0;
    unsigned char* data = CCFileUtils::get()->getFileData(tmpPngStr.c_str(), "rb", &len);
    if (!data || len == 0) {
        delete[] data;
        paths::removeQuiet(tmpPng);
        return {};
    }

    auto reloaded = geode::Ref<CCImage>::adopt(new CCImage());
    bool ok = reloaded->initWithImageData(data, len, CCImage::kFmtPng);
    delete[] data;
    paths::removeQuiet(tmpPng);

    if (!ok || !reloaded->getData() || reloaded->getWidth() <= 0 || reloaded->getHeight() <= 0) return {};
    return reloaded;
}

}

CCImage* cropImage(CCImage* src, int cropX, int cropY, int cropW, int cropH) {
    if (!src) return nullptr;

    size_t bufSize = pixelops::rgbaSize(cropW, cropH);
    if (bufSize == 0) return nullptr;

    std::vector<unsigned char> buf(bufSize);
    if (!pixelops::crop(src->getData(), src->getWidth(), src->getHeight(), cropX, cropY, cropW, cropH, buf.data()))
        return nullptr;

    auto cropped = geode::Ref<CCImage>::adopt(new CCImage());
    if (!cropped->initWithImageData(buf.data(), static_cast<int>(bufSize), CCImage::kFmtRawData, cropW, cropH, 8,
                                   false))
        return nullptr;
    return cropped.take();
}

bool saveGIF(CCImage* img, bool transparentBg, std::string const& path) {
    if (!img) return false;
    int w = img->getWidth();
    int h = img->getHeight();
    if (w <= 0 || h <= 0) return false;

    auto reloaded = reloadThroughPng(img, "ore_tmp_gif.png");
    if (!reloaded) return false;

    w = reloaded->getWidth();
    h = reloaded->getHeight();

    std::vector<unsigned char> gct(pixelops::c_gifPaletteSize);
    pixelops::writeGifPalette(gct.data());

    std::vector<unsigned char> indices(static_cast<size_t>(w) * h);
    bool hasTransparent = pixelops::quantizeToGif(reloaded->getData(), w, h, transparentBg, indices.data());

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out.write("GIF89a", 6);
    out.put(static_cast<char>(w & 0xFF));
    out.put(static_cast<char>((w >> 8) & 0xFF));
    out.put(static_cast<char>(h & 0xFF));
    out.put(static_cast<char>((h >> 8) & 0xFF));
    out.put(static_cast<char>(0xF7));
    out.put(static_cast<char>(0));
    out.put(static_cast<char>(0));
    out.write(reinterpret_cast<char*>(gct.data()), gct.size());

    if (hasTransparent) {
        out.put(static_cast<char>(0x21));
        out.put(static_cast<char>(0xF9));
        out.put(static_cast<char>(0x04));
        out.put(static_cast<char>(0x01));
        out.put(static_cast<char>(0)); out.put(static_cast<char>(0));
        out.put(static_cast<char>(0));
        out.put(static_cast<char>(0));
    }

    out.put(static_cast<char>(0x2C));
    out.put(static_cast<char>(0)); out.put(static_cast<char>(0));
    out.put(static_cast<char>(0)); out.put(static_cast<char>(0));
    out.put(static_cast<char>(w & 0xFF));
    out.put(static_cast<char>((w >> 8) & 0xFF));
    out.put(static_cast<char>(h & 0xFF));
    out.put(static_cast<char>((h >> 8) & 0xFF));
    out.put(static_cast<char>(0));

    int minCodeSize = 8;
    int clearCode = 1 << minCodeSize;
    int endCode = clearCode + 1;
    int codeSize = minCodeSize + 1;
    out.put(static_cast<char>(minCodeSize));

    struct BitWriter {
        std::vector<unsigned char> bytes;
        int currentByte = 0;
        int bitsInByte = 0;
        void write(int code, int bits) {
            for (int i = 0; i < bits; ++i) {
                if (code & (1 << i)) currentByte |= (1 << bitsInByte);
                if (++bitsInByte == 8) {
                    bytes.push_back(static_cast<unsigned char>(currentByte));
                    currentByte = 0;
                    bitsInByte = 0;
                }
            }
        }
        void flush() {
            if (bitsInByte > 0) {
                bytes.push_back(static_cast<unsigned char>(currentByte));
                currentByte = 0;
                bitsInByte = 0;
            }
        }
    } writer;

    size_t pixelCount = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < pixelCount; ++i) {
        writer.write(clearCode, codeSize);
        writer.write(indices[i], codeSize);
    }
    writer.write(endCode, codeSize);
    writer.flush();

    size_t pos = 0;
    while (pos < writer.bytes.size()) {
        size_t len = std::min<size_t>(255, writer.bytes.size() - pos);
        out.put(static_cast<char>(len));
        out.write(reinterpret_cast<char*>(writer.bytes.data() + pos), static_cast<std::streamsize>(len));
        pos += len;
    }
    out.put(static_cast<char>(0));
    out.put(static_cast<char>(0x3B));
    bool result = out.good();
    return result;
}

bool saveJPEG(CCImage* img, int quality, std::string const& path) {
    if (!img) return false;
    int w = img->getWidth();
    int h = img->getHeight();
    if (w <= 0 || h <= 0) return false;

    auto reloaded = reloadThroughPng(img, "ore_tmp_jpeg.png");
    if (!reloaded) return false;

    w = reloaded->getWidth();
    h = reloaded->getHeight();

    std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
    pixelops::rgbaToRgb(reloaded->getData(), w * h, rgb.data());

    return stbi_write_jpg(path.c_str(), w, h, 3, rgb.data(), std::clamp(quality, 1, 100)) != 0;
}

bool saveImage(CCImage* img, ExportFormat format, ExportCompression compression, int quality, bool transparentBg, std::string& path) {
    if (!img) return false;

    (void)compression;
    std::string ext = formatExtension(format);
    auto p = std::filesystem::path(path);
    if (p.extension().string() != fmt::format(".{}", ext)) {
        path = (p.parent_path() / (p.stem().string() + fmt::format(".{}", ext))).string();
    }
    if (!paths::ensureParentDir(std::filesystem::path(path))) return false;

    switch (format) {
        case ExportFormat::PNG:
            return img->saveToFile(path.c_str(), false);
        case ExportFormat::JPEG:
            return saveJPEG(img, quality, path);
        case ExportFormat::GIF:
            return saveGIF(img, transparentBg, path);
        default:
            break;
    }

    geode::log::warn("Format {} is not yet supported by this build; falling back to PNG.",
        formatName(format));
    path = (p.parent_path() / (p.stem().string() + ".png")).string();
    return img->saveToFile(path.c_str(), true);
}

bool findVisibleBounds(CCImage* img, int& outX, int& outY, int& outW, int& outH) {
    if (!img) return false;
    return pixelops::visibleBounds(img->getData(), img->getWidth(), img->getHeight(), outX, outY, outW, outH);
}

