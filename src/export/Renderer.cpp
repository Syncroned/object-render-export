#include "export/Renderer.hpp"

#include "export/Shading.hpp"
#include "image/ImageExport.hpp"
#include "image/PixelOps.hpp"
#include "utils/MediaScan.hpp"
#include "utils/NodeTree.hpp"
#include "utils/Paths.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

using namespace geode::prelude;
using namespace cocos2d;

bool renderAndSave(CCSprite* sprite, ExportRequest const& request, AdditiveParts const& additive,
    CCRect const& worldBounds, CCArray* selected, LevelEditorLayer* lel, std::string& path) {
    CCSize contentSize = sprite->getContentSize();
    sprite->setPosition(contentSize / 2);

    float csf = CCDirector::get()->getContentScaleFactor();
    CCSize rtPoints = {static_cast<float>(request.width) / csf, static_cast<float>(request.height) / csf};

    auto rt = geode::Ref<CCRenderTexture>(CCRenderTexture::create(
        rtPoints.width, rtPoints.height, kCCTexture2DPixelFormat_RGBA8888));
    if (!rt) return false;

    float scale = std::min(rtPoints.width / contentSize.width, rtPoints.height / contentSize.height);

    rt->beginWithClear(0, 0, 0, request.transparentBg ? 0 : 1);
    sprite->setScale(scale);
    sprite->setPosition(rtPoints / 2);
    sprite->visit();
    rt->end();

    auto img = geode::Ref<CCImage>::adopt(rt->newCCImage(true));

    auto renderTransparent = [&]() -> geode::Ref<CCImage> {
        auto pass = geode::Ref<CCRenderTexture>(CCRenderTexture::create(
            rtPoints.width, rtPoints.height, kCCTexture2DPixelFormat_RGBA8888));
        if (!pass) return {};
        pass->beginWithClear(0, 0, 0, 0);
        sprite->setScale(scale);
        sprite->setPosition(rtPoints / 2);
        sprite->visit();
        pass->end();
        return geode::Ref<CCImage>::adopt(pass->newCCImage(true));
    };

    auto fixupAdditiveAlpha = [&](geode::Ref<CCImage>& target) {
        if (!target) return;

        std::vector<CCSprite*> allSprites;
        nodetree::collectSprites(sprite, {}, allSprites);

        const ccBlendFunc noOp = {GL_ZERO, GL_ONE};
        std::vector<std::pair<CCSprite*, ccBlendFunc>> masked;
        for (auto* s : allSprites) {
            if (additive.sprites.count(s)) {
                masked.push_back({s, s->getBlendFunc()});
                s->setBlendFunc(noOp);
            }
        }

        for (auto* ps : additive.particles) ps->setVisible(false);
        for (auto* g : additive.gradients) g->setVisible(false);

        auto covImg = renderTransparent();

        for (auto& [s, func] : masked) s->setBlendFunc(func);
        for (auto* ps : additive.particles) ps->setVisible(true);
        for (auto* g : additive.gradients) g->setVisible(true);

        if (covImg && covImg->getWidth() == target->getWidth() && covImg->getHeight() == target->getHeight()) {
            pixelops::reconstructAlpha(target->getData(), covImg->getData(),
                target->getWidth() * target->getHeight());
        }
    };

    if (img && request.transparentBg) {
        fixupAdditiveAlpha(img);
        pixelops::unpremultiply(img->getData(), img->getWidth() * img->getHeight());
    }

    if (img && request.includeShaders && lel->m_shaderLayer) {
        applyShadersGPU(img, lel->m_shaderLayer, selected, worldBounds);
    }

    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
    bool hasCrop = false;

    if (request.cropToVisible && img) {
        if (request.transparentBg) {
            hasCrop = findVisibleBounds(img, cropX, cropY, cropW, cropH);
        } else if (auto trimImg = renderTransparent()) {
            fixupAdditiveAlpha(trimImg);
            hasCrop = findVisibleBounds(trimImg, cropX, cropY, cropW, cropH);
        }

        hasCrop = hasCrop && cropW > 0 && cropH > 0;
    }

    if (!img || !paths::ensureParentDir(std::filesystem::path(path))) return false;

    auto cropped = [&](geode::Ref<CCImage> source) -> geode::Ref<CCImage> {
        if (!hasCrop) return source;
        auto sub = geode::Ref<CCImage>::adopt(cropImage(source, cropX, cropY, cropW, cropH));
        return sub ? sub : source;
    };

    bool saved = saveImage(cropped(img), request.format, request.compression, request.quality,
        request.transparentBg, path);
    if (saved) media::triggerMediaScan(path);

    if (Mod::get()->getSettingValue<bool>("export-alpha-mask")) {
        geode::Ref<CCImage> maskImg = img;
        if (!request.transparentBg) {
            maskImg = renderTransparent();
            fixupAdditiveAlpha(maskImg);
        }

        if (maskImg) {
            pixelops::alphaToGrayscale(maskImg->getData(), maskImg->getWidth() * maskImg->getHeight());

            std::filesystem::path srcPath = path;
            std::filesystem::path alphaPath = srcPath.parent_path() / (srcPath.stem().string() + "_alpha.png");

            if (cropped(maskImg)->saveToFile(alphaPath.string().c_str(), false)) {
                media::triggerMediaScan(alphaPath.string());
            }
        }
    }

    return saved;
}
