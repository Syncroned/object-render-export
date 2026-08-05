#include "export/Shading.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;
using namespace cocos2d;

void applyShadersGPU(geode::Ref<CCImage>& img, ShaderLayer* shaderLayer, CCArray* selectedObjects,
    CCRect const& worldBounds) {
    if (!img || !shaderLayer) return;
    geode::Ref<CCSprite> sprite{shaderLayer->m_sprite};
    auto* shaderProgram = shaderLayer->m_shader;
    if (!sprite || !shaderProgram) return;

    int imgW = img->getWidth();
    int imgH = img->getHeight();
    if (imgW <= 0 || imgH <= 0) return;

    auto texture = geode::Ref<CCTexture2D>::adopt(new CCTexture2D());
    texture->initWithData(img->getData(), kCCTexture2DPixelFormat_RGBA8888,
        imgW, imgH, CCSizeMake(imgW, imgH));
    texture->setAntiAliasTexParameters();

    geode::Ref<CCTexture2D> origTexture{sprite->getTexture()};
    CCGLProgram* origShader = sprite->getShaderProgram();
    ccBlendFunc origBlend = sprite->getBlendFunc();
    CCRect origRect = sprite->getTextureRect();
    CCSize origContentSize = sprite->getContentSize();
    CCPoint origPosition = sprite->getPosition();
    CCPoint origAnchor = sprite->getAnchorPoint();
    float origScaleX = sprite->getScaleX();
    float origScaleY = sprite->getScaleY();
    float origRotation = sprite->getRotation();
    bool origVisible = sprite->isVisible();

    CCSize origScreenSize = shaderLayer->m_screenSize;
    CCSize origTargetTextureSize = shaderLayer->m_targetTextureSize;
    CCSize origTargetTextureSizeExtra = shaderLayer->m_targetTextureSizeExtra;
    GJShaderState origState = shaderLayer->m_state;


    sprite->setTexture(texture);
    sprite->setTextureRect(CCRectMake(0, 0, imgW, imgH));
    sprite->setContentSize(CCSizeMake(imgW, imgH));
    sprite->setAnchorPoint(CCPointZero);
    sprite->setPosition(CCPointZero);
    sprite->setScaleX(1.f);
    sprite->setScaleY(1.f);
    sprite->setRotation(0.f);
    sprite->setVisible(true);
    sprite->setShaderProgram(shaderProgram);
    sprite->setBlendFunc({GL_ONE, GL_ZERO});

    shaderLayer->m_screenSize = CCSizeMake(imgW, imgH);
    shaderLayer->m_targetTextureSize = CCSizeMake(imgW, imgH);
    shaderLayer->m_targetTextureSizeExtra = CCSizeMake(imgW, imgH);
    shaderLayer->m_state.m_textureScaleX = 1.f;
    shaderLayer->m_state.m_textureScaleY = 1.f;

    auto posToExportUV = [&](CCPoint const& worldPos) -> CCPoint {
        if (worldBounds.size.width <= 0.f || worldBounds.size.height <= 0.f)
            return CCPointMake(0.5f, 0.5f);
        float u = (worldPos.x - worldBounds.origin.x) / worldBounds.size.width;
        float v = (worldPos.y - worldBounds.origin.y) / worldBounds.size.height;
        return CCPointMake(u, v);
    };

    auto findTargetUV = [&](int targetID, bool& found) -> CCPoint {
        found = false;
        if (!selectedObjects || targetID == 0) return CCPointMake(0.5f, 0.5f);
        for (unsigned int i = 0; i < selectedObjects->count(); ++i) {
            auto* obj = static_cast<GameObject*>(selectedObjects->objectAtIndex(i));
            if (obj && obj->m_uniqueID == targetID) {
                found = true;
                return posToExportUV(obj->getPosition());
            }
        }
        return CCPointMake(0.5f, 0.5f);
    };

    auto centerFor = [&](int targetID, bool active, char const* name) -> CCPoint {
        bool found = false;
        CCPoint uv = findTargetUV(targetID, found);
        if (active && !found && targetID != 0) {
            geode::log::warn("Shader '{}': target object {} not in export selection; centering on the export image.", name, targetID);
        }
        return uv;
    };

    GJShaderState& s = shaderLayer->m_state;
    s.m_shockWaveCenter = centerFor(s.m_shockWaveTargetID, s.m_shockWaveTarget, "Shock Wave");
    s.m_shockLineCenter = centerFor(s.m_shockLineTargetID, s.m_shockLineTarget, "Shock Line");
    s.m_bulgeCenter = centerFor(s.m_bulgeTargetID, s.m_bulgeValue > 0.f, "Bulge");

    int pinchTargetID = 0;
    bool pinchActive = false;
    if (s.m_pinchTargetEnabledX) {
        pinchTargetID = s.m_pinchTargetIDX;
        pinchActive = true;
    } else if (s.m_pinchTargetEnabledY) {
        pinchTargetID = s.m_pinchTargetIDY;
        pinchActive = true;
    }
    s.m_pinchCenter = centerFor(pinchTargetID, pinchActive, "Pinch");

    s.m_radialBlurCenter = centerFor(s.m_radialBlurTargetID, s.m_radialBlurTarget, "Radial Blur");
    s.m_lensCircleCenter = centerFor(s.m_lensCircleTargetID, s.m_lensCircleTargetID != 0, "Lens Circle");

    shaderLayer->preCommonShader();
    shaderLayer->preColorChangeShader();
    shaderLayer->preGrayscaleShader();
    shaderLayer->preSepiaShader();
    shaderLayer->preInvertColorShader();
    shaderLayer->preHueShiftShader();
    shaderLayer->preBulgeShader();
    shaderLayer->preChromaticShader();
    shaderLayer->preChromaticGlitchShader();
    shaderLayer->preGlitchShader();
    shaderLayer->preShockWaveShader();
    shaderLayer->preShockLineShader();
    shaderLayer->preLensCircleShader();
    shaderLayer->preRadialBlurShader();
    shaderLayer->prePinchShader();
    shaderLayer->prePixelateShader();
    shaderLayer->preMotionBlurShader();
    shaderLayer->preSplitScreenShader();

    float csf = CCDirector::get()->getContentScaleFactor();
    CCSize rtSize = {static_cast<float>(imgW) / csf, static_cast<float>(imgH) / csf};

    auto rt = geode::Ref<CCRenderTexture>(CCRenderTexture::create(
        static_cast<int>(rtSize.width), static_cast<int>(rtSize.height),
        kCCTexture2DPixelFormat_RGBA8888
    ));
    if (rt) {
        rt->beginWithClear(0, 0, 0, 0);
        sprite->visit();
        rt->end();

        auto shadered = geode::Ref<CCImage>::adopt(rt->newCCImage(false));

        if (shadered && shadered->getData() &&
            shadered->getWidth() == imgW && shadered->getHeight() == imgH) {
            img = shadered;
        }
    }

    sprite->setTexture(origTexture);
    sprite->setTextureRect(origRect);
    sprite->setContentSize(origContentSize);
    sprite->setPosition(origPosition);
    sprite->setAnchorPoint(origAnchor);
    sprite->setScaleX(origScaleX);
    sprite->setScaleY(origScaleY);
    sprite->setRotation(origRotation);
    sprite->setVisible(origVisible);
    sprite->setShaderProgram(origShader);
    sprite->setBlendFunc(origBlend);

    shaderLayer->m_screenSize = origScreenSize;
    shaderLayer->m_targetTextureSize = origTargetTextureSize;
    shaderLayer->m_targetTextureSizeExtra = origTargetTextureSizeExtra;
    shaderLayer->m_state = origState;

}
