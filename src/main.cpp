#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <cocos2d.h>
#include <algorithm>
#include <vector>
#include <map>
#include <functional>
#include <utility>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <cstdint>

#include "stb_image_write.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <comdef.h>
#include <shlobj.h>
#endif

#include "ExportPopup.hpp"

using namespace geode::prelude;
using namespace cocos2d;

#ifdef _WIN32
static std::string showFolderPickerDialog() {
    std::string result;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return result;
    }

    IFileOpenDialog* pFileOpen = NULL;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                          IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFileOpen->GetOptions(&dwOptions);
        pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);

        hr = pFileOpen->Show(NULL);

        if (SUCCEEDED(hr)) {
            IShellItem* pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFolderPath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
                if (SUCCEEDED(hr)) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, pszFolderPath, -1, NULL, 0, NULL, NULL);
                    if (len > 0) {
                        result.resize(len);
                        WideCharToMultiByte(CP_UTF8, 0, pszFolderPath, -1, &result[0], len, NULL, NULL);
                        result.resize(len - 1);
                    }
                    CoTaskMemFree(pszFolderPath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    CoUninitialize();
    return result;
}
#endif

struct ObjEntry {
    GameObject* obj;
    int         zLayer;
    int         zOrder;
};

static std::vector<ObjEntry> sortedObjects(CCArray* objects) {
    std::vector<ObjEntry> entries;
    entries.reserve(objects->count());

    for (int i = 0; i < (int)objects->count(); ++i) {
        auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        entries.push_back({
            obj,
            (int)obj->m_zLayer,
            obj->getZOrder()
        });
    }

    std::stable_sort(entries.begin(), entries.end(), [](ObjEntry const& a, ObjEntry const& b) {
        if (a.zLayer != b.zLayer) return a.zLayer < b.zLayer;
        return a.zOrder < b.zOrder;
    });

    return entries;
}

static ccColor4F resolveChannelColor(int channelId) {
    auto* lel = LevelEditorLayer::get();
    if (!lel) return {1.f, 1.f, 1.f, 1.f};

    auto* effectManager = lel->m_effectManager;
    if (!effectManager) return {1.f, 1.f, 1.f, 1.f};
    
    auto* ca = effectManager->getColorAction(channelId);
    if (!ca) return {1.f, 1.f, 1.f, 1.f};

    ccColor3B col = ca->m_color;
    float opacity = ca->m_currentOpacity;
    return {col.r / 255.f, col.g / 255.f, col.b / 255.f, opacity};
}

static CCRect worldBounds(std::vector<ObjEntry> const& entries) {
    float minX = 1e9f, minY = 1e9f;
    float maxX = -1e9f, maxY = -1e9f;

    for (auto& e : entries) {
        CCPoint pos = e.obj->getPosition();
        CCSize  sz  = e.obj->getContentSize();
        float   scX = e.obj->getScaleX();
        float   scY = e.obj->getScaleY();

        float hw = sz.width * scX * 0.5f;
        float hh = sz.height * scY * 0.5f;

        minX = std::min(minX, pos.x - hw);
        minY = std::min(minY, pos.y - hh);
        maxX = std::max(maxX, pos.x + hw);
        maxY = std::max(maxY, pos.y + hh);
    }

    return CCRect{minX, minY, maxX - minX, maxY - minY};
}

static CCImage* cropImage(CCImage* src, int cropX, int cropY, int cropW, int cropH) {
    if (!src || cropW <= 0 || cropH <= 0) return nullptr;
    int srcW = src->getWidth();
    int srcH = src->getHeight();
    if (cropX < 0 || cropY < 0 || cropX + cropW > srcW || cropY + cropH > srcH)
        return nullptr;

    int newDataLen = cropW * cropH * 4;
    auto* cropped = new CCImage();
    auto* buf = new unsigned char[newDataLen];
    unsigned char* srcData = src->getData();

    for (int y = 0; y < cropH; ++y) {
        memcpy(
            buf + y * cropW * 4,
            srcData + (y + cropY) * srcW * 4 + cropX * 4,
            cropW * 4
        );
    }

    bool ok = cropped->initWithImageData(buf, newDataLen, cocos2d::CCImage::kFmtRawData, cropW, cropH, 8, false);
    delete[] buf;
    if (!ok) {
        cropped->release();
        return nullptr;
    }
    return cropped;
}

static bool saveGIF(CCImage* img, bool transparentBg, std::string const& path) {
    if (!img) return false;
    int w = img->getWidth();
    int h = img->getHeight();
    if (w <= 0 || h <= 0) return false;

    std::string tmpPng = (Mod::get()->getSaveDir() / "ore_tmp_gif.png").string();
    if (!img->saveToFile(tmpPng.c_str(), false)) return false;

    CCImage* reloaded = new CCImage();
    unsigned long tmpLen = 0;
    unsigned char* tmpData = CCFileUtils::get()->getFileData(tmpPng.c_str(), "rb", &tmpLen);
    if (!tmpData || tmpLen == 0) {
        if (tmpData) delete[] tmpData;
        reloaded->release();
        std::filesystem::remove(tmpPng);
        return false;
    }
    bool ok = reloaded->initWithImageData(tmpData, tmpLen, CCImage::kFmtPng);
    delete[] tmpData;
    std::filesystem::remove(tmpPng);
    if (!ok) {
        reloaded->release();
        return false;
    }

    w = reloaded->getWidth();
    h = reloaded->getHeight();
    unsigned char* d = reloaded->getData();
    if (!d || w <= 0 || h <= 0) {
        reloaded->release();
        return false;
    }

    // 6x6x6 color cube plus a transparent index at 0.
    const int paletteSize = 256;
    std::vector<unsigned char> gct(paletteSize * 3, 0);
    int idx = 1;
    for (int r = 0; r < 6; ++r) {
        for (int g = 0; g < 6; ++g) {
            for (int b = 0; b < 6; ++b) {
                if (idx >= paletteSize) break;
                gct[idx * 3 + 0] = static_cast<unsigned char>(r * 51);
                gct[idx * 3 + 1] = static_cast<unsigned char>(g * 51);
                gct[idx * 3 + 2] = static_cast<unsigned char>(b * 51);
                ++idx;
            }
        }
    }

    // 8x8 Bayer dither matrix to fake transparency levels in GIF's single transparent index.
    static const unsigned char dither[64] = {
         0, 32,  8, 40,  2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44,  4, 36, 14, 46,  6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
         3, 35, 11, 43,  1, 33,  9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47,  7, 39, 13, 45,  5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    };

    std::vector<unsigned char> indices(static_cast<size_t>(w) * h);
    bool hasTransparent = false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int p = (y * w + x) * 4;
            unsigned char a = d[p + 3];
            int threshold = (dither[(y & 7) * 8 + (x & 7)] * 255) / 64;
            if (transparentBg && a <= threshold) {
                indices[y * w + x] = 0;
                hasTransparent = true;
            } else {
                int r = (d[p + 0] * 5) / 255;
                int g = (d[p + 1] * 5) / 255;
                int b = (d[p + 2] * 5) / 255;
                indices[y * w + x] = static_cast<unsigned char>(1 + r * 36 + g * 6 + b);
            }
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Header, logical screen descriptor, global color table.
    out.write("GIF89a", 6);
    out.put(static_cast<char>(w & 0xFF));
    out.put(static_cast<char>((w >> 8) & 0xFF));
    out.put(static_cast<char>(h & 0xFF));
    out.put(static_cast<char>((h >> 8) & 0xFF));
    out.put(static_cast<char>(0xF7)); // global color table flag, 256 colors
    out.put(static_cast<char>(0));    // background color index
    out.put(static_cast<char>(0));    // aspect ratio
    out.write(reinterpret_cast<char*>(gct.data()), gct.size());

    if (hasTransparent) {
        out.put(static_cast<char>(0x21)); // extension introducer
        out.put(static_cast<char>(0xF9)); // graphic control label
        out.put(static_cast<char>(0x04)); // block size
        out.put(static_cast<char>(0x01)); // transparent color flag set
        out.put(static_cast<char>(0)); out.put(static_cast<char>(0)); // delay
        out.put(static_cast<char>(0)); // transparent color index
        out.put(static_cast<char>(0)); // block terminator
    }

    // Image descriptor.
    out.put(static_cast<char>(0x2C));
    out.put(static_cast<char>(0)); out.put(static_cast<char>(0)); // left
    out.put(static_cast<char>(0)); out.put(static_cast<char>(0)); // top
    out.put(static_cast<char>(w & 0xFF));
    out.put(static_cast<char>((w >> 8) & 0xFF));
    out.put(static_cast<char>(h & 0xFF));
    out.put(static_cast<char>((h >> 8) & 0xFF));
    out.put(static_cast<char>(0)); // no local color table

    // Image data with LZW minimum code size 8.
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

    // Uncompressed LZW stream: clear + index, clear + index, ..., end.
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
    out.put(static_cast<char>(0)); // sub-block terminator
    out.put(static_cast<char>(0x3B)); // trailer
    bool result = out.good();
    reloaded->release();
    return result;
}

static bool saveJPEG(CCImage* img, int quality, std::string const& path) {
    if (!img) return false;
    int w = img->getWidth();
    int h = img->getHeight();
    if (w <= 0 || h <= 0) return false;

    std::string tmpPng = (Mod::get()->getSaveDir() / "ore_tmp_jpeg.png").string();
    if (!img->saveToFile(tmpPng.c_str(), false)) return false;

    CCImage* reloaded = new CCImage();
    unsigned long tmpLen = 0;
    unsigned char* tmpData = CCFileUtils::get()->getFileData(tmpPng.c_str(), "rb", &tmpLen);
    if (!tmpData || tmpLen == 0) {
        if (tmpData) delete[] tmpData;
        reloaded->release();
        std::filesystem::remove(tmpPng);
        return false;
    }
    bool ok = reloaded->initWithImageData(tmpData, tmpLen, CCImage::kFmtPng);
    delete[] tmpData;
    std::filesystem::remove(tmpPng);
    if (!ok) {
        reloaded->release();
        return false;
    }

    w = reloaded->getWidth();
    h = reloaded->getHeight();
    unsigned char* d = reloaded->getData();
    if (!d || w <= 0 || h <= 0) {
        reloaded->release();
        return false;
    }

    std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
    for (int p = 0; p < w * h; ++p) {
        rgb[p * 3 + 0] = d[p * 4 + 0];
        rgb[p * 3 + 1] = d[p * 4 + 1];
        rgb[p * 3 + 2] = d[p * 4 + 2];
    }

    int q = std::clamp(quality, 1, 100);
    bool result = stbi_write_jpg(path.c_str(), w, h, 3, rgb.data(), q) != 0;
    reloaded->release();
    return result;
}

static bool saveImage(CCImage* img, ExportFormat format, ExportCompression compression, int quality, bool transparentBg, std::string& path) {
    if (!img) return false;

    (void)compression;
    std::string ext = formatExtension(format);
    auto p = std::filesystem::path(path);
    if (p.extension().string() != fmt::format(".{}", ext)) {
        path = (p.parent_path() / (p.stem().string() + fmt::format(".{}", ext))).string();
    }
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

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
        s_formatNames[static_cast<int>(format)]);
    path = (p.parent_path() / (p.stem().string() + ".png")).string();
    return img->saveToFile(path.c_str(), true);
}

static bool findVisibleBounds(CCImage* img, int& outX, int& outY, int& outW, int& outH) {
    if (!img) return false;
    int w = img->getWidth();
    int h = img->getHeight();
    unsigned char* d = img->getData();
    if (!d) return false;

    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (d[(y * w + x) * 4 + 3]) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0) return false;
    outX = minX;
    outY = minY;
    outW = maxX - minX + 1;
    outH = maxY - minY + 1;
    return true;
}

bool exportSelectedObjects(int width, int height, bool transparentBg, bool cropToVisible,
    ExportFormat format, ExportCompression compression, int quality, std::string& path) {
    auto* lel = LevelEditorLayer::get();
    if (!lel) return false;

    auto* ui = lel->m_editorUI;
    CCArray* selected = ui->getSelectedObjects();
    if (!selected || selected->count() == 0) return false;

    struct ObjectData {
        GameObject* obj;
        bool isVisible;
        GLubyte opacity;
        bool shouldBlendBase;
        bool shouldBlendDetail;
        int zLayer;
        int zOrder;
        int uniqueID;
        // Draw order from scene graph: chain of (zOrder, sibling index) from root to object
        std::vector<std::pair<int, int>> drawKey;
    };
    std::vector<ObjectData> originalData;

    auto* effectManager = lel->m_effectManager;

    auto channelBlends = [&](int channelId) -> bool {
        if (!effectManager) return false;
        auto* ca = effectManager->getColorAction(channelId);
        return ca && ca->m_blending;
    };

    auto computeDrawKey = [](CCNode* node) {
        std::vector<std::pair<int, int>> key;
        for (CCNode* n = node; n != nullptr; n = n->getParent()) {
            CCNode* p = n->getParent();
            int idx = 0;
            if (p && p->getChildren()) {
                unsigned int found = p->getChildren()->indexOfObject(n);
                idx = (found == UINT_MAX) ? 0 : static_cast<int>(found);
            }
            key.emplace_back(n->getZOrder(), idx);
        }
        std::reverse(key.begin(), key.end());
        return key;
    };

    for (int i = 0; i < selected->count(); i++) {
        GameObject* obj = static_cast<GameObject*>(selected->objectAtIndex(i));
        ObjectData data;
        data.obj = obj;
        data.isVisible = obj->isVisible();
        data.opacity = obj->getOpacity();
        data.zLayer = (int)obj->m_zLayer;
        data.zOrder = obj->m_zOrder;
        data.uniqueID = obj->m_uniqueID;
        data.drawKey = computeDrawKey(obj);

        int baseChan = obj->m_baseColor ? obj->m_baseColor->m_colorID : -1;
        int detailChan = obj->m_detailColor ? obj->m_detailColor->m_colorID : -1;
        data.shouldBlendBase = obj->m_shouldBlendBase ||
            (obj->m_baseColor && channelBlends(baseChan));
        data.shouldBlendDetail = obj->m_shouldBlendDetail ||
            (obj->m_detailColor && channelBlends(detailChan));

        originalData.push_back(data);
    }

    std::stable_sort(originalData.begin(), originalData.end(),
        [](ObjectData const& a, ObjectData const& b) {
            if (a.zLayer != b.zLayer) return a.zLayer < b.zLayer;
            if (a.drawKey != b.drawKey) return a.drawKey < b.drawKey;
            return a.uniqueID < b.uniqueID;
        });

    std::ostringstream buffer;
    for (const auto& data : originalData) {
        if (data.isVisible) {
            buffer << data.obj->getSaveString(lel).c_str() << ";";
        }
    }
    std::string objString = buffer.str();

    CCArray* objArray = CCArray::create();
    CCSprite* sprite = ui->spriteFromObjectString(objString, false, false, INT_MAX, objArray, nullptr, nullptr);
    lel->updateObjectColors(objArray);
    struct DrawItem {
        CCNode* node;
        CCSprite* sprite;
        ccBlendFunc func;
        bool useBlend;
        int zLayer;
        int group;
        int subOrder;
        std::vector<std::pair<int, int>> drawKey;
        bool isSprite;
    };
    std::vector<DrawItem> drawItems;

    // Match the original export brightness: additive uses GL_SRC_ALPHA (not GL_ONE)
    // as the source factor so blended colours are scaled by their alpha instead of
    // being added at full intensity, and the glow sprite is left untouched below.
    const ccBlendFunc additive = {GL_SRC_ALPHA, GL_ONE};

    auto collectSprites = [&](CCNode* root, std::vector<CCNode*> const& skips,
                              std::vector<CCSprite*>& out, bool includeRoot = true) {
        std::function<void(CCNode*)> rec = [&](CCNode* node) {
            if (!node) return;
            for (auto* sk : skips) {
                if (node == sk) return;
            }
            if (includeRoot || node != root) {
                if (auto* spr = geode::cast::typeinfo_cast<CCSprite*>(node)) {
                    out.push_back(spr);
                }
            }
            if (auto* children = node->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    rec(static_cast<CCNode*>(children->objectAtIndex(i)));
                }
            }
        };
        rec(root);
    };

    int visibleIndex = 0;
    for (const auto& data : originalData) {
        if (data.isVisible && visibleIndex < objArray->count()) {
            GameObject* gameObject = static_cast<GameObject*>(objArray->objectAtIndex(visibleIndex));

            gameObject->setOpacity(data.opacity);

            bool baseBlend = data.shouldBlendBase;
            bool detailBlend = data.shouldBlendDetail;

            int baseGroup = 1;
            if (baseBlend && detailBlend) baseGroup = 0;
            else if (baseBlend && !detailBlend && gameObject->m_colorSprite) baseGroup = 2;
            else if (baseBlend) baseGroup = 0;

            int detailGroup = detailBlend ? 0 : 1;

            drawItems.push_back({gameObject, nullptr, additive, baseBlend, data.zLayer, baseGroup, 0, data.drawKey, false});

            if (baseBlend) {
                std::vector<CCSprite*> baseSprites;
                collectSprites(gameObject,
                    {gameObject->m_colorSprite, gameObject->m_glowSprite}, baseSprites, false);
                for (auto* s : baseSprites) s->setBlendFunc(additive);
            }

            if (gameObject->m_colorSprite) {
                if (detailBlend) {
                    std::vector<CCSprite*> detailSprites;
                    collectSprites(gameObject->m_colorSprite, {}, detailSprites, true);
                    for (auto* s : detailSprites) s->setBlendFunc(additive);
                }
                if (baseBlend ^ detailBlend) {
                    drawItems.push_back({gameObject->m_colorSprite, gameObject->m_colorSprite, additive, detailBlend, data.zLayer, detailGroup, 1, data.drawKey, true});
                }
            }

            visibleIndex++;
        }
    }

    std::stable_sort(drawItems.begin(), drawItems.end(),
        [](DrawItem const& a, DrawItem const& b) {
            if (a.zLayer != b.zLayer) return a.zLayer < b.zLayer;
            if (a.group != b.group) return a.group < b.group;
            if (a.drawKey != b.drawKey) return a.drawKey < b.drawKey;
            return a.subOrder < b.subOrder;
        });

    int drawZ = 0;
    for (auto& item : drawItems) {
        if (item.isSprite) {
            CCSprite* s = item.sprite;
            CCNode* parent = s->getParent();
            if (!parent) {
                if (item.useBlend) s->setBlendFunc(item.func);
                continue;
            }

            CCPoint worldPos = parent->convertToWorldSpace(s->getPosition());
            CCPoint localPos = sprite->convertToNodeSpace(worldPos);

            s->retain();
            s->removeFromParent();
            s->setPosition(localPos);
            if (item.useBlend) s->setBlendFunc(item.func);
            sprite->addChild(s, drawZ);
            s->release();
        } else {
            item.node->setZOrder(drawZ);
            if (item.useBlend) {
                if (auto* spr = geode::cast::typeinfo_cast<CCSprite*>(item.node)) {
                    spr->setBlendFunc(item.func);
                }
            }
        }
        ++drawZ;
    }

    CCSize contentSize = sprite->getContentSize();
    sprite->setPosition(contentSize / 2);

    CCSize renderSize = {static_cast<float>(width), static_cast<float>(height)};

    float csf = CCDirector::get()->getContentScaleFactor();
    CCSize rtPoints = { renderSize.width / csf, renderSize.height / csf };

    auto* rt = CCRenderTexture::create(
        rtPoints.width,
        rtPoints.height,
        kCCTexture2DPixelFormat_RGBA8888
    );
    if (!rt) return false;
    rt->retain();

    rt->beginWithClear(0, 0, 0, transparentBg ? 0 : 1);

    float scaleX = rtPoints.width / contentSize.width;
    float scaleY = rtPoints.height / contentSize.height;
    float scale = std::min(scaleX, scaleY);

    sprite->setScale(scale);
    sprite->setPosition(rtPoints / 2);
    sprite->visit();
    
    rt->end();

    CCImage* img = rt->newCCImage(true);

    // Alpha reconstruction: additive sprites corrupt the alpha channel in transparent
    // exports. Render a coverage pass with additive sprites hidden, then
    // alpha = max(coverageAlpha, luminance(colourRGB)).
    if (img && transparentBg) {
        std::vector<CCSprite*> allSprites;
        collectSprites(sprite, {}, allSprites);

        std::vector<CCSprite*> hiddenAdditive;
        for (auto* s : allSprites) {
            if (s->getBlendFunc().dst == GL_ONE && s->isVisible()) {
                s->setVisible(false);
                hiddenAdditive.push_back(s);
            }
        }
        auto* rtCov = CCRenderTexture::create(
            rtPoints.width,
            rtPoints.height,
            kCCTexture2DPixelFormat_RGBA8888
        );
        if (rtCov) {
            rtCov->retain();
            rtCov->beginWithClear(0, 0, 0, 0);
            sprite->setScale(scale);
            sprite->setPosition(rtPoints / 2);
            sprite->visit();
            rtCov->end();

            CCImage* covImg = rtCov->newCCImage(true);
            if (covImg &&
                covImg->getWidth()  == img->getWidth() &&
                covImg->getHeight() == img->getHeight()) {
                unsigned char* colorData = img->getData();
                unsigned char* covData   = covImg->getData();
                int pixelCount = img->getWidth() * img->getHeight();
                for (int p = 0; p < pixelCount; ++p) {
                    unsigned char r = colorData[p * 4 + 0];
                    unsigned char g = colorData[p * 4 + 1];
                    unsigned char b = colorData[p * 4 + 2];
                    unsigned char lum = std::max({r, g, b});
                    unsigned char cov = covData[p * 4 + 3];
                    colorData[p * 4 + 3] = std::max(cov, lum);
                }
            }
            if (covImg) covImg->release();
            rtCov->release();
        }

        for (auto* s : hiddenAdditive) s->setVisible(true);
    }

    // Crop to visible bounds. Opaque exports need a separate transparent render.
    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
    bool hasCrop = false;

    if (cropToVisible && img) {
        if (transparentBg) {
            hasCrop = findVisibleBounds(img, cropX, cropY, cropW, cropH);
        } else {
            auto* rtTrim = CCRenderTexture::create(
                rtPoints.width, rtPoints.height,
                kCCTexture2DPixelFormat_RGBA8888
            );
            if (rtTrim) {
                rtTrim->retain();
                rtTrim->beginWithClear(0, 0, 0, 0);
                sprite->setScale(scale);
                sprite->setPosition(rtPoints / 2);
                sprite->visit();
                rtTrim->end();

                CCImage* trimImg = rtTrim->newCCImage(true);
                rtTrim->release();

                if (trimImg) {
                    std::vector<CCSprite*> allSprites;
                    collectSprites(sprite, {}, allSprites);
                    std::vector<CCSprite*> hiddenAdditive;
                    for (auto* s : allSprites) {
                        if (s->getBlendFunc().dst == GL_ONE && s->isVisible()) {
                            s->setVisible(false);
                            hiddenAdditive.push_back(s);
                        }
                    }
                    auto* rtCov = CCRenderTexture::create(
                        rtPoints.width, rtPoints.height,
                        kCCTexture2DPixelFormat_RGBA8888
                    );
                    if (rtCov) {
                        rtCov->retain();
                        rtCov->beginWithClear(0, 0, 0, 0);
                        sprite->setScale(scale);
                        sprite->setPosition(rtPoints / 2);
                        sprite->visit();
                        rtCov->end();
                        CCImage* covImg = rtCov->newCCImage(true);
                        if (covImg &&
                            covImg->getWidth() == trimImg->getWidth() &&
                            covImg->getHeight() == trimImg->getHeight()) {
                            unsigned char* colorData = trimImg->getData();
                            unsigned char* covData = covImg->getData();
                            int pixelCount = trimImg->getWidth() * trimImg->getHeight();
                            for (int p = 0; p < pixelCount; ++p) {
                                unsigned char r = colorData[p * 4 + 0];
                                unsigned char g = colorData[p * 4 + 1];
                                unsigned char b = colorData[p * 4 + 2];
                                unsigned char lum = std::max({r, g, b});
                                unsigned char cov = covData[p * 4 + 3];
                                colorData[p * 4 + 3] = std::max(cov, lum);
                            }
                        }
                        if (covImg) covImg->release();
                        rtCov->release();
                    }
                    for (auto* s : hiddenAdditive) s->setVisible(true);

                    hasCrop = findVisibleBounds(trimImg, cropX, cropY, cropW, cropH);
                    trimImg->release();
                }
            }
        }

        hasCrop = hasCrop && cropW > 0 && cropH > 0;
    }

    bool saved = false;
    if (img) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());

        if (hasCrop) {
            CCImage* cropped = cropImage(img, cropX, cropY, cropW, cropH);
            if (cropped) {
                saved = saveImage(cropped, format, compression, quality, transparentBg, path);
                triggerMediaScan(path);
                cropped->release();
            } else {
                saved = saveImage(img, format, compression, quality, transparentBg, path);
                triggerMediaScan(path);
            }
        } else {
            saved = saveImage(img, format, compression, quality, transparentBg, path);
            if (saved) {
                triggerMediaScan(path);
            }
        }

        if (Mod::get()->getSettingValue<bool>("export-alpha-mask")) {
            CCImage* maskImg = img;
            bool ownsMaskImg = false;

            if (!transparentBg) {
                auto* rtMask = CCRenderTexture::create(
                    rtPoints.width, rtPoints.height,
                    kCCTexture2DPixelFormat_RGBA8888
                );
                if (rtMask) {
                    rtMask->retain();
                    rtMask->beginWithClear(0, 0, 0, 0);
                    sprite->setScale(scale);
                    sprite->setPosition(rtPoints / 2);
                    sprite->visit();
                    rtMask->end();

                    maskImg = rtMask->newCCImage(true);
                    rtMask->release();
                    ownsMaskImg = true;

                    if (maskImg) {
                        std::vector<CCSprite*> allSprites;
                        collectSprites(sprite, {}, allSprites);

                        std::vector<CCSprite*> hiddenAdditive;
                        for (auto* s : allSprites) {
                            if (s->getBlendFunc().dst == GL_ONE && s->isVisible()) {
                                s->setVisible(false);
                                hiddenAdditive.push_back(s);
                            }
                        }

                        auto* rtCov = CCRenderTexture::create(
                            rtPoints.width, rtPoints.height,
                            kCCTexture2DPixelFormat_RGBA8888
                        );
                        if (rtCov) {
                            rtCov->retain();
                            rtCov->beginWithClear(0, 0, 0, 0);
                            sprite->setScale(scale);
                            sprite->setPosition(rtPoints / 2);
                            sprite->visit();
                            rtCov->end();

                            CCImage* covImg = rtCov->newCCImage(true);
                            if (covImg &&
                                covImg->getWidth() == maskImg->getWidth() &&
                                covImg->getHeight() == maskImg->getHeight()) {
                                unsigned char* colorData = maskImg->getData();
                                unsigned char* covData = covImg->getData();
                                int pixelCount = maskImg->getWidth() * maskImg->getHeight();
                                for (int p = 0; p < pixelCount; ++p) {
                                    unsigned char r = colorData[p * 4 + 0];
                                    unsigned char g = colorData[p * 4 + 1];
                                    unsigned char b = colorData[p * 4 + 2];
                                    unsigned char lum = std::max({r, g, b});
                                    unsigned char cov = covData[p * 4 + 3];
                                    colorData[p * 4 + 3] = std::max(cov, lum);
                                }
                            }
                            if (covImg) covImg->release();
                            rtCov->release();
                        }

                        for (auto* s : hiddenAdditive) s->setVisible(true);
                    }
                }
            }

            if (maskImg) {
                unsigned char* d = maskImg->getData();
                int pixelCount = maskImg->getWidth() * maskImg->getHeight();
                for (int p = 0; p < pixelCount; ++p) {
                    unsigned char a = d[p * 4 + 3];
                    d[p * 4 + 0] = a;
                    d[p * 4 + 1] = a;
                    d[p * 4 + 2] = a;
                    d[p * 4 + 3] = 255;
                }

                std::filesystem::path srcPath = path;
                std::filesystem::path alphaPath =
                    srcPath.parent_path() / (srcPath.stem().string() + "_alpha.png");

                if (hasCrop) {
                    CCImage* croppedMask = cropImage(maskImg, cropX, cropY, cropW, cropH);
                    if (croppedMask) {
                        if (croppedMask->saveToFile(alphaPath.string().c_str(), false)) {
                            triggerMediaScan(alphaPath.string());
                        }
                        croppedMask->release();
                    } else {
                        if (maskImg->saveToFile(alphaPath.string().c_str(), false)) {
                            triggerMediaScan(alphaPath.string());
                        }
                    }
                } else {
                    if (maskImg->saveToFile(alphaPath.string().c_str(), false)) {
                        triggerMediaScan(alphaPath.string());
                    }
                }

                if (ownsMaskImg) maskImg->release();
            }
        }

        img->release();
    }

    rt->release();

    return saved;
}

$execute {
#ifdef _WIN32
    auto mod = Mod::get();
    auto exportPath = mod->getSettingValue<std::filesystem::path>("export-path");
    if (exportPath.empty()) {
        PWSTR picsPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &picsPath))) {
            int len = WideCharToMultiByte(CP_UTF8, 0, picsPath, -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                std::string pathStr(len, 0);
                WideCharToMultiByte(CP_UTF8, 0, picsPath, -1, &pathStr[0], len, NULL, NULL);
                pathStr.resize(len - 1);
                mod->setSettingValue<std::filesystem::path>("export-path", std::filesystem::path(pathStr));
            }
            CoTaskMemFree(picsPath);
        }
    }
#endif
}

class $modify(MyEditorUI, EditorUI) {
    void createMoveMenu() {
        EditorUI::createMoveMenu();

        auto* sprite = CCSprite::createWithSpriteFrameName("ExportButton.png"_spr);
        sprite->setScale(1.0f);

        auto* btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(MyEditorUI::onOpenExportPopup)
        );
        m_editButtonBar->m_buttonArray->addObject(btn);

        auto rows = GameManager::sharedState()->getIntGameVariable("0049");
        auto cols = GameManager::sharedState()->getIntGameVariable("0050");
        m_editButtonBar->reloadItems(rows, cols);
    }

    void onOpenExportPopup(CCObject*) {
        auto* sel = this->getSelectedObjects();
        if (!sel || sel->count() == 0) {
            FLAlertLayer::create(
                "Nothing Selected",
                "Select at least one object before exporting.",
                "OK"
            )->show();
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string randomStr = std::to_string(timestamp);
        std::string fileName = "object_export-" + randomStr + ".png";

#ifdef _WIN32
        bool directSave = Mod::get()->getSettingValue<bool>("direct-save");

        if (directSave) {
            auto exportPath = Mod::get()->getSettingValue<std::filesystem::path>("export-path");
            if (exportPath.empty()) {
                PWSTR picsPath = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &picsPath))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, picsPath, -1, NULL, 0, NULL, NULL);
                    if (len > 0) {
                        std::string pathStr(len, 0);
                        WideCharToMultiByte(CP_UTF8, 0, picsPath, -1, &pathStr[0], len, NULL, NULL);
                        pathStr.resize(len - 1);
                        exportPath = std::filesystem::path(pathStr);
                    }
                    CoTaskMemFree(picsPath);
                }
            }
            auto filePath = exportPath / fileName;

            std::string filePathStr = geode::utils::string::pathToString(filePath);
            geode::log::debug("Direct save to: {}", filePathStr);

            auto* popup = ExportPopup::create();
            if (popup) {
                geode::log::debug("ExportPopup created successfully, setting path and showing");
                popup->setExportPath(filePathStr);
                popup->show();
            } else {
                geode::log::error("Failed to create ExportPopup");
            }
        } else {
            std::string folderPath = showFolderPickerDialog();
            if (folderPath.empty()) {
                return;
            }

            auto filePath = std::filesystem::path(folderPath) / fileName;
            std::string filePathStr = geode::utils::string::pathToString(filePath);
            geode::log::debug("Creating ExportPopup with path: {}", filePathStr);

            auto* popup = ExportPopup::create();
            if (popup) {
                geode::log::debug("ExportPopup created successfully, setting path and showing");
                popup->setExportPath(filePathStr);
                popup->show();
            } else {
                geode::log::error("Failed to create ExportPopup");
            }
        }
#else
        auto exportPath = Mod::get()->getSettingValue<std::filesystem::path>("export-path");
        if (exportPath.empty()) {
            exportPath = std::filesystem::path("/storage/emulated/0/Download/Exports");
        }

        auto exportsDir = exportPath;

        std::filesystem::create_directories(exportsDir);

        auto filePath = exportsDir / fileName;
        std::string filePathStr = geode::utils::string::pathToString(filePath);
        geode::log::debug("Auto-exporting to: {}", filePathStr);

        auto* popup = ExportPopup::create();
        if (popup) {
            geode::log::debug("ExportPopup created successfully, setting path and showing");
            popup->setExportPath(filePathStr);
            popup->show();
        } else {
            geode::log::error("Failed to create ExportPopup");
        }
#endif
    }
};
