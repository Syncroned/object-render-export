#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include "export/ExportSettings.hpp"

using namespace geode::prelude;

class ExportSettingsPopup : public geode::Popup {
public:
    static ExportSettingsPopup* create();

protected:
    ExportFormat m_format = ExportFormat::PNG;
    ExportCompression m_compression = ExportCompression::Lossless;
    int m_quality = 90;
    bool m_transparent = false;
    bool m_crop = false;
    bool m_includeGradients = false;
    bool m_includeShaders = false;

    std::vector<CCMenuItemSpriteExtra*> m_formatButtons;
    CCMenuItemSpriteExtra* m_compressionBtn = nullptr;
    Slider* m_qualitySlider = nullptr;
    CCLabelBMFont* m_qualityLabel = nullptr;
    CCLabelBMFont* m_qualityValue = nullptr;
    CCMenuItemToggler* m_transparentToggle = nullptr;
    CCMenuItemSpriteExtra* m_transparencyInfoBtn = nullptr;
    CCLabelBMFont* m_transparentLabel = nullptr;
    CCMenuItemToggler* m_cropToggle = nullptr;
    CCMenuItemToggler* m_gradientsToggle = nullptr;
    CCMenuItemToggler* m_shadersToggle = nullptr;

    void loadSettings();
    void persist();
    void showTooltip(char const* title, char const* text);
    void validateFormat();
    void updateFormatButtons();
    void updateCompressionButton();
    void updateQualitySlider();
    void updateTransparencyToggle();
    void updateAll();
    bool setup();

    void onFormatButton(CCObject* sender);
    void onCompression(CCObject*);
    void onQuality(CCObject*);
    void onToggleBg(CCObject*);
    void onTransparencyInfo(CCObject*);
    void onToggleCrop(CCObject*);
    void onToggleGradients(CCObject*);
    void onToggleShaders(CCObject*);
    void onShadersInfo(CCObject*);
};
