#include "ui/ExportSettingsPopup.hpp"
#include "ui/PopupWidgets.hpp"

using namespace geode::prelude;

ExportSettingsPopup* ExportSettingsPopup::create() {
    auto* ret = new ExportSettingsPopup();
    if (ret && ret->init(360.f, 300.f)) {
        if (!ret->setup()) {
            delete ret;
            return nullptr;
        }
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void ExportSettingsPopup::loadSettings() {
    auto s = loadExportSettings();
    m_format = s.format;
    m_compression = s.compression;
    m_quality = s.quality;
    m_transparent = s.transparent;
    m_crop = s.crop;
    m_includeGradients = s.includeGradients;
    m_includeShaders = s.includeShaders;
}

void ExportSettingsPopup::persist() {
    ExportSettings s;
    s.format = m_format;
    s.compression = m_compression;
    s.quality = m_quality;
    s.transparent = m_transparent;
    s.crop = m_crop;
    s.includeGradients = m_includeGradients;
    s.includeShaders = m_includeShaders;
    saveExportSettings(s);
}

void ExportSettingsPopup::showTooltip(char const* title, char const* text) {
    geode::createQuickPopup(title, text, "OK", nullptr, 350.f,
        [](FLAlertLayer*, bool) {});
}

void ExportSettingsPopup::validateFormat() {
    ExportSettings s;
    s.format = m_format;
    s.compression = m_compression;
    s.quality = m_quality;
    s.transparent = m_transparent;
    s.crop = m_crop;
    s.includeGradients = m_includeGradients;
    s.includeShaders = m_includeShaders;
    auto [reconciled, messages] = reconcileExportSettings(s);
    for (auto const& msg : messages) {
        showTooltip("Settings Adjusted", msg.c_str());
    }
    m_format = reconciled.format;
    m_compression = reconciled.compression;
    m_quality = reconciled.quality;
    m_transparent = reconciled.transparent;
    m_crop = reconciled.crop;
    m_includeGradients = reconciled.includeGradients;
    m_includeShaders = reconciled.includeShaders;
    persist();
}

void ExportSettingsPopup::updateFormatButtons() {
    for (auto* btn : m_formatButtons) {
        int idx = btn->getTag();
        bool selected = (idx == static_cast<int>(m_format));
        const char* texture = selected ? "GJ_button_02.png" : "GJ_button_04.png";
        auto* newSprite = ButtonSprite::create(
            formatName(static_cast<ExportFormat>(idx)), 50, true,
            "bigFont.fnt", texture, 28.f, 0.45f
        );
        btn->setNormalImage(newSprite);
        btn->setSelectedImage(newSprite);
    }
}

void ExportSettingsPopup::updateCompressionButton() {
    auto* newSprite = ButtonSprite::create(
        compressionName(m_compression), 90, true,
        "bigFont.fnt", "GJ_button_02.png", 30.f, 0.5f
    );
    m_compressionBtn->setNormalImage(newSprite);
    m_compressionBtn->setSelectedImage(newSprite);
}

void ExportSettingsPopup::updateQualitySlider() {
    bool lossy = (m_compression == ExportCompression::Lossy);
    bool transparentAllowed = !m_transparent;
    bool visible = lossy && transparentAllowed;
    m_qualitySlider->setVisible(visible);
    m_qualityLabel->setVisible(visible);
    m_qualityValue->setVisible(visible);
    if (visible) {
        m_qualitySlider->setValue(static_cast<float>(m_quality) / 100.0f);
        m_qualityValue->setString(fmt::format("{}%", m_quality).c_str());
    }
}

void ExportSettingsPopup::updateTransparencyToggle() {
    bool supports = formatSupportsTransparency(m_format);
    m_transparentToggle->setEnabled(supports);
    m_transparentToggle->setOpacity(supports ? 255 : 120);
    m_transparentLabel->setOpacity(supports ? 255 : 120);
    m_transparencyInfoBtn->setVisible(!supports);
    m_transparentToggle->toggle(m_transparent);
}

void ExportSettingsPopup::updateAll() {
    updateFormatButtons();
    updateCompressionButton();
    updateQualitySlider();
    updateTransparencyToggle();
}

bool ExportSettingsPopup::setup() {
    this->setTitle("Export Settings");
    auto* layer = m_mainLayer;

    loadSettings();
    validateFormat();

    constexpr float W = 360.f;
    constexpr float CX = W / 2.f;

    auto* formatLabel = CCLabelBMFont::create("Format", "bigFont.fnt");
    formatLabel->setScale(0.5f);
    formatLabel->setPosition({CX, 260.f});
    layer->addChild(formatLabel);

    auto* formatGridMenu = CCMenu::create();
    formatGridMenu->setPosition({0.f, 0.f});
    constexpr float gridStartX = CX - 75.f;
    constexpr float gridStartY = 230.f;
    constexpr float gapX = 75.f;
    constexpr float gapY = 30.f;
    for (int i = 0; i < static_cast<int>(ExportFormat::COUNT); ++i) {
        int col = i % 3;
        int row = i / 3;
        const char* texture = (i == static_cast<int>(m_format)) ? "GJ_button_02.png" : "GJ_button_04.png";
        auto* btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(formatName(static_cast<ExportFormat>(i)), 50, true, "bigFont.fnt", texture, 28.f, 0.45f),
            this,
            menu_selector(ExportSettingsPopup::onFormatButton)
        );
        btn->setTag(i);
        btn->setPosition({gridStartX + col * gapX, gridStartY - row * gapY});
        formatGridMenu->addChild(btn);
        m_formatButtons.push_back(btn);
    }
    layer->addChild(formatGridMenu);

    m_gradientsToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ExportSettingsPopup::onToggleGradients), 0.7f
    );
    m_gradientsToggle->toggle(m_includeGradients);
    addToggleRow(layer, "Include Gradients", 185.f, m_gradientsToggle);

    m_shadersToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ExportSettingsPopup::onToggleShaders), 0.7f
    );
    m_shadersToggle->toggle(m_includeShaders);

    auto* shaderInfoBtn = makeIconButton("GJ_infoIcon_001.png", 0.5f, this, menu_selector(ExportSettingsPopup::onShadersInfo));
    addToggleRow(layer, "Include Shaders", 158.f, m_shadersToggle, shaderInfoBtn);

    m_transparentToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ExportSettingsPopup::onToggleBg), 0.7f
    );
    m_transparentToggle->toggle(m_transparent);

    m_transparencyInfoBtn = makeIconButton("GJ_infoIcon_001.png", 0.5f, this, menu_selector(ExportSettingsPopup::onTransparencyInfo));
    m_transparencyInfoBtn->setVisible(!formatSupportsTransparency(m_format));
    m_transparentLabel =
        addToggleRow(layer, "Transparent BG", 108.f, m_transparentToggle, m_transparencyInfoBtn);

    m_cropToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ExportSettingsPopup::onToggleCrop), 0.7f
    );
    m_cropToggle->toggle(m_crop);
    addToggleRow(layer, "Snap to Pixels", 85.f, m_cropToggle);

    auto* compressionLabel = CCLabelBMFont::create("Compression", "bigFont.fnt");
    compressionLabel->setScale(0.5f);
    compressionLabel->setPosition({100.f, 55.f});
    layer->addChild(compressionLabel);

    m_compressionBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create(compressionName(m_compression), 90, true, "bigFont.fnt", "GJ_button_02.png", 30.f, 0.5f),
        this,
        menu_selector(ExportSettingsPopup::onCompression)
    );
    auto* compressionMenu = CCMenu::create();
    compressionMenu->setPosition({220.f, 55.f});
    compressionMenu->addChild(m_compressionBtn);
    layer->addChild(compressionMenu);

    m_qualityLabel = CCLabelBMFont::create("Quality", "bigFont.fnt");
    m_qualityLabel->setScale(0.45f);
    m_qualityLabel->setPosition({55.f, 30.f});
    m_qualityLabel->setAnchorPoint({0.f, 0.5f});
    layer->addChild(m_qualityLabel);

    m_qualitySlider = Slider::create(this, menu_selector(ExportSettingsPopup::onQuality), 0.55f);
    m_qualitySlider->setValue(static_cast<float>(m_quality) / 100.0f);
    m_qualitySlider->setPosition({175.f, 30.f});
    layer->addChild(m_qualitySlider);

    m_qualityValue = CCLabelBMFont::create(fmt::format("{}%", m_quality).c_str(), "bigFont.fnt");
    m_qualityValue->setScale(0.4f);
    m_qualityValue->setPosition({305.f, 30.f});
    m_qualityValue->setAnchorPoint({1.f, 0.5f});
    layer->addChild(m_qualityValue);

    updateTransparencyToggle();
    updateQualitySlider();

    return true;
}

void ExportSettingsPopup::onFormatButton(CCObject* sender) {
    int fmt = static_cast<CCNode*>(sender)->getTag();
    if (fmt == static_cast<int>(m_format)) return;
    m_format = static_cast<ExportFormat>(fmt);
    validateFormat();
    updateAll();
}

void ExportSettingsPopup::onCompression(CCObject*) {
    int comp = static_cast<int>(m_compression);
    bool lossy = formatSupportsLossy(m_format);
    bool lossless = formatSupportsLossless(m_format);

    if (lossy && lossless) {
        comp = (comp == 0) ? 1 : 0;
    } else if (lossy) {
        comp = 0;
        showTooltip("Compression Locked", "This format only supports lossy compression.");
    } else if (lossless) {
        comp = 1;
        showTooltip("Compression Locked", "This format only supports lossless compression.");
    }

    auto newComp = static_cast<ExportCompression>(comp);
    if (m_transparent && newComp == ExportCompression::Lossy) {
        newComp = ExportCompression::Lossless;
        showTooltip("Compression Locked", "This format cannot combine lossy compression with transparency.");
    }

    m_compression = newComp;
    persist();
    updateCompressionButton();
    updateQualitySlider();
}

void ExportSettingsPopup::onQuality(CCObject*) {
    m_quality = std::clamp(static_cast<int>(m_qualitySlider->getValue() * 100.0f + 0.5f), 0, 100);
    persist();
    m_qualityValue->setString(fmt::format("{}%", m_quality).c_str());
}

void ExportSettingsPopup::onToggleBg(CCObject*) {
    if (!formatSupportsTransparency(m_format)) {
        showTooltip("Not Supported", "This format does not support transparency.");
        return;
    }
    bool next = !m_transparent;
    if (next && m_compression == ExportCompression::Lossy) {
        m_compression = ExportCompression::Lossless;
        persist();
        updateCompressionButton();
        showTooltip("Compression Changed", "This format cannot combine lossy compression with transparency. Switched to lossless.");
    }
    m_transparent = next;
    persist();
    updateQualitySlider();
}

void ExportSettingsPopup::onTransparencyInfo(CCObject*) {
    showTooltip("Transparency", "This image format does not support transparency. Use PNG or GIF if you need a transparent background.");
}

void ExportSettingsPopup::onToggleCrop(CCObject*) {
    m_crop = !m_crop;
    persist();
}

void ExportSettingsPopup::onToggleGradients(CCObject*) {
    m_includeGradients = !m_includeGradients;
    persist();
}

void ExportSettingsPopup::onToggleShaders(CCObject*) {
    m_includeShaders = !m_includeShaders;
    persist();
}

void ExportSettingsPopup::onShadersInfo(CCObject*) {
    geode::createQuickPopup(
        "Include Shaders",
        "Applies all currently active shaders to the export: colour effects, "
        "Shock Wave, Shock Line, Bulge, Pinch, Lens Circle, Radial/Motion Blur, "
        "Pixelate, Glitch, Chromatic, Grayscale, Sepia, Invert and Hue Shift.\n\n"
        "Screen/camera-anchored effects are centred on the export image. "
        "Object-anchored effects use the selected target object if it is part of "
        "the export; otherwise they fall back to the export centre and a warning "
        "is written to the log. Select the shader's centre object if you want it "
        "to be positioned exactly.",
        "OK",
        nullptr,
        520.f,
        [](FLAlertLayer*, bool){}
    );
}
