#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

enum class ExportFormat {
    PNG,
    JPEG,
    GIF,
    COUNT
};

enum class ExportCompression {
    Lossy,
    Lossless,
    COUNT
};

bool exportSelectedObjects(int width, int height, bool transparentBg, bool cropToVisible,
    ExportFormat format, ExportCompression compression, int quality, std::string& path);

#ifdef GEODE_IS_ANDROID
extern "C" void triggerMediaScan(std::string const& path);
#else
inline void triggerMediaScan(std::string const& path) {}
#endif

static const char* s_formatNames[] = {
    "PNG", "JPEG", "GIF"
};

static const char* s_compressionNames[] = {
    "Lossy", "Lossless"
};

static bool formatSupportsTransparency(ExportFormat fmt) {
    switch (fmt) {
        case ExportFormat::JPEG: return false;
        case ExportFormat::PNG: return true;
        case ExportFormat::GIF: return true;
        default: return false;
    }
}

static bool formatSupportsLossy(ExportFormat fmt) {
    switch (fmt) {
        case ExportFormat::JPEG: return true;
        case ExportFormat::PNG: return false;
        case ExportFormat::GIF: return false;
        default: return false;
    }
}

static bool formatSupportsTransparentLossy(ExportFormat fmt) {
    return false;
}

static bool formatSupportsLossless(ExportFormat fmt) {
    switch (fmt) {
        case ExportFormat::JPEG: return false;
        case ExportFormat::PNG: return true;
        case ExportFormat::GIF: return true;
        default: return false;
    }
}

static const char* formatExtension(ExportFormat fmt) {
    switch (fmt) {
        case ExportFormat::JPEG: return "jpg";
        case ExportFormat::PNG: return "png";
        case ExportFormat::GIF: return "gif";
        default: return "png";
    }
}

class ExportSettingsPopup : public geode::Popup {
public:
    static ExportSettingsPopup* create() {
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

protected:
    ExportFormat m_format = ExportFormat::PNG;
    ExportCompression m_compression = ExportCompression::Lossless;
    int m_quality = 90;
    bool m_transparent = false;
    bool m_crop = false;

    std::vector<CCMenuItemSpriteExtra*> m_formatButtons;
    CCMenuItemSpriteExtra* m_compressionBtn = nullptr;
    Slider* m_qualitySlider = nullptr;
    CCLabelBMFont* m_qualityLabel = nullptr;
    CCLabelBMFont* m_qualityValue = nullptr;
    CCMenuItemToggler* m_transparentToggle = nullptr;
    CCMenuItemSpriteExtra* m_transparencyInfoBtn = nullptr;
    CCLabelBMFont* m_transparentLabel = nullptr;
    CCMenuItemToggler* m_cropToggle = nullptr;

    void loadSettings() {
        int fmt = Mod::get()->getSavedValue<int>("export-format", static_cast<int>(ExportFormat::PNG));
        if (fmt < 0 || fmt >= static_cast<int>(ExportFormat::COUNT)) fmt = static_cast<int>(ExportFormat::PNG);
        m_format = static_cast<ExportFormat>(fmt);

        int comp = Mod::get()->getSavedValue<int>("export-compression", static_cast<int>(ExportCompression::Lossless));
        if (comp < 0 || comp >= static_cast<int>(ExportCompression::COUNT)) comp = static_cast<int>(ExportCompression::Lossless);
        m_compression = static_cast<ExportCompression>(comp);

        m_quality = Mod::get()->getSavedValue<int>("export-quality", 90);
        if (m_quality < 0) m_quality = 0;
        if (m_quality > 100) m_quality = 100;

        m_transparent = Mod::get()->getSavedValue<bool>("export-transparent-bg", false);
        m_crop = Mod::get()->getSavedValue<bool>("export-crop-visible", false);
    }

    void saveFormat() {
        Mod::get()->setSavedValue<int>("export-format", static_cast<int>(m_format));
    }

    void saveCompression() {
        Mod::get()->setSavedValue<int>("export-compression", static_cast<int>(m_compression));
    }

    void saveQuality() {
        Mod::get()->setSavedValue<int>("export-quality", m_quality);
    }

    void showTooltip(char const* title, char const* text) {
        FLAlertLayer::create(title, text, "OK")->show();
    }

    void validateFormat() {
        bool lossy = formatSupportsLossy(m_format);
        bool lossless = formatSupportsLossless(m_format);

        if (m_compression == ExportCompression::Lossy && !lossy) {
            if (lossless) {
                m_compression = ExportCompression::Lossless;
                showTooltip("Format Changed", "This format does not support lossy compression. Switched to lossless.");
            } else {
                m_compression = ExportCompression::Lossless;
            }
        } else if (m_compression == ExportCompression::Lossless && !lossless) {
            if (lossy) {
                m_compression = ExportCompression::Lossy;
                showTooltip("Format Changed", "This format does not support lossless compression. Switched to lossy.");
            } else {
                m_compression = ExportCompression::Lossy;
            }
        }

        saveCompression();

        if (m_transparent && !formatSupportsTransparency(m_format)) {
            m_transparent = false;
            Mod::get()->setSavedValue<bool>("export-transparent-bg", false);
            showTooltip("Transparency Disabled", "This format does not support transparency. Transparent BG turned off.");
        }

        if (m_transparent && m_compression == ExportCompression::Lossy && !formatSupportsTransparentLossy(m_format)) {
            m_compression = ExportCompression::Lossless;
            saveCompression();
            showTooltip("Compression Changed", "This format cannot combine lossy compression with transparency. Switched to lossless.");
        }
    }

    void updateFormatButtons() {
        for (auto* btn : m_formatButtons) {
            int idx = btn->getTag();
            bool selected = (idx == static_cast<int>(m_format));
            const char* texture = selected ? "GJ_button_02.png" : "GJ_button_04.png";
            auto* newSprite = ButtonSprite::create(
                s_formatNames[idx], 50, true,
                "bigFont.fnt", texture, 28.f, 0.45f
            );
            btn->setNormalImage(newSprite);
            btn->setSelectedImage(newSprite);
        }
    }

    void updateCompressionButton() {
        auto* newSprite = ButtonSprite::create(
            s_compressionNames[static_cast<int>(m_compression)], 90, true,
            "bigFont.fnt", "GJ_button_02.png", 30.f, 0.5f
        );
        m_compressionBtn->setNormalImage(newSprite);
        m_compressionBtn->setSelectedImage(newSprite);
    }

    void updateQualitySlider() {
        bool lossy = (m_compression == ExportCompression::Lossy);
        bool transparentAllowed = !m_transparent || formatSupportsTransparentLossy(m_format);
        bool visible = lossy && transparentAllowed;
        m_qualitySlider->setVisible(visible);
        m_qualityLabel->setVisible(visible);
        m_qualityValue->setVisible(visible);
        if (visible) {
            m_qualitySlider->setValue(static_cast<float>(m_quality) / 100.0f);
            m_qualityValue->setString(fmt::format("{}%", m_quality).c_str());
        }
    }

    void updateTransparencyToggle() {
        bool supports = formatSupportsTransparency(m_format);
        m_transparentToggle->setEnabled(supports);
        m_transparentToggle->setOpacity(supports ? 255 : 120);
        m_transparentLabel->setOpacity(supports ? 255 : 120);
        m_transparencyInfoBtn->setVisible(!supports);
        m_transparentToggle->toggle(m_transparent);
    }

    void updateAll() {
        updateFormatButtons();
        updateCompressionButton();
        updateQualitySlider();
        updateTransparencyToggle();
    }

    bool setup() {
        this->setTitle("Export Settings");
        auto* layer = m_mainLayer;

        loadSettings();
        validateFormat();

        constexpr float W = 360.f;
        constexpr float H = 300.f;
        constexpr float CX = W / 2.f;
        constexpr float CY = H / 2.f;

        auto* formatLabel = CCLabelBMFont::create("Format", "bigFont.fnt");
        formatLabel->setScale(0.5f);
        formatLabel->setPosition({CX, 260.f});
        layer->addChild(formatLabel);

        auto* formatGridMenu = CCMenu::create();
        formatGridMenu->setPosition({0.f, 0.f});
        float gridStartX = CX - 75.f;
        float gridStartY = 230.f;
        float gapX = 75.f;
        float gapY = 30.f;
        for (int i = 0; i < static_cast<int>(ExportFormat::COUNT); ++i) {
            int col = i % 3;
            int row = i / 3;
            const char* texture = (i == static_cast<int>(m_format)) ? "GJ_button_02.png" : "GJ_button_04.png";
            auto* btn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(s_formatNames[i], 50, true, "bigFont.fnt", texture, 28.f, 0.45f),
                this,
                menu_selector(ExportSettingsPopup::onFormatButton)
            );
            btn->setTag(i);
            btn->setPosition({gridStartX + col * gapX, gridStartY - row * gapY});
            formatGridMenu->addChild(btn);
            m_formatButtons.push_back(btn);
        }
        layer->addChild(formatGridMenu);

        auto* compressionLabel = CCLabelBMFont::create("Compression", "bigFont.fnt");
        compressionLabel->setScale(0.5f);
        compressionLabel->setPosition({100.f, 55.f});
        layer->addChild(compressionLabel);

        m_compressionBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(s_compressionNames[static_cast<int>(m_compression)], 90, true, "bigFont.fnt", "GJ_button_02.png", 30.f, 0.5f),
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

        m_transparentLabel = CCLabelBMFont::create("Transparent BG", "bigFont.fnt");
        m_transparentLabel->setScale(0.45f);
        m_transparentLabel->setPosition({115.f, 108.f});
        layer->addChild(m_transparentLabel);

        m_transparentToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(ExportSettingsPopup::onToggleBg), 0.7f
        );
        m_transparentToggle->toggle(m_transparent);

        auto* infoSprite = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        infoSprite->setScale(0.5f);
        m_transparencyInfoBtn = CCMenuItemSpriteExtra::create(
            infoSprite,
            this,
            menu_selector(ExportSettingsPopup::onTransparencyInfo)
        );
        m_transparencyInfoBtn->setVisible(!formatSupportsTransparency(m_format));

        auto* toggleMenu = CCMenu::create();
        toggleMenu->setPosition({205.f, 108.f});
        toggleMenu->addChild(m_transparentToggle);
        layer->addChild(toggleMenu);

        auto* infoMenu = CCMenu::create();
        infoMenu->setPosition({270.f, 108.f});
        infoMenu->addChild(m_transparencyInfoBtn);
        layer->addChild(infoMenu);

        auto* cropLabel = CCLabelBMFont::create("Snap to Pixels", "bigFont.fnt");
        cropLabel->setScale(0.45f);
        cropLabel->setPosition({115.f, 85.f});
        layer->addChild(cropLabel);

        m_cropToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(ExportSettingsPopup::onToggleCrop), 0.7f
        );
        m_cropToggle->toggle(m_crop);

        auto* cropMenu = CCMenu::create();
        cropMenu->setPosition({205.f, 85.f});
        cropMenu->addChild(m_cropToggle);
        layer->addChild(cropMenu);

        updateTransparencyToggle();
        updateQualitySlider();

        return true;
    }

    void onFormatButton(CCObject* sender) {
        int fmt = static_cast<CCNode*>(sender)->getTag();
        if (fmt == static_cast<int>(m_format)) return;
        m_format = static_cast<ExportFormat>(fmt);
        saveFormat();
        validateFormat();
        updateAll();
    }

    void onCompression(CCObject*) {
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
        if (m_transparent && newComp == ExportCompression::Lossy && !formatSupportsTransparentLossy(m_format)) {
            newComp = ExportCompression::Lossless;
            showTooltip("Compression Locked", "This format cannot combine lossy compression with transparency.");
        }

        m_compression = newComp;
        saveCompression();
        updateCompressionButton();
        updateQualitySlider();
    }

    void onQuality(CCObject*) {
        int value = static_cast<int>(m_qualitySlider->getValue() * 100.0f + 0.5f);
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        m_quality = value;
        saveQuality();
        m_qualityValue->setString(fmt::format("{}%", m_quality).c_str());
    }

    void onToggleBg(CCObject*) {
        if (!formatSupportsTransparency(m_format)) {
            showTooltip("Not Supported", "This format does not support transparency.");
            return;
        }
        bool next = !m_transparent;
        if (next && m_compression == ExportCompression::Lossy && !formatSupportsTransparentLossy(m_format)) {
            m_compression = ExportCompression::Lossless;
            saveCompression();
            updateCompressionButton();
            showTooltip("Compression Changed", "This format cannot combine lossy compression with transparency. Switched to lossless.");
        }
        m_transparent = next;
        Mod::get()->setSavedValue<bool>("export-transparent-bg", m_transparent);
        updateQualitySlider();
    }

    void onTransparencyInfo(CCObject*) {
        showTooltip("Transparency", "This image format does not support transparency. Use PNG or GIF if you need a transparent background.");
    }

    void onToggleCrop(CCObject*) {
        m_crop = !m_crop;
        Mod::get()->setSavedValue<bool>("export-crop-visible", m_crop);
    }
};

class ExportPopup : public geode::Popup {
public:
    static ExportPopup* create() {
        geode::log::debug("ExportPopup::create() called");
        auto* ret = new ExportPopup();
        geode::log::debug("ExportPopup allocated: {}", fmt::ptr(ret));
        if (ret && ret->init(340.f, 240.f)) {
            geode::log::debug("ExportPopup::init() succeeded");
            if (!ret->setup()) {
                geode::log::error("ExportPopup::setup() failed");
                delete ret;
                return nullptr;
            }
            geode::log::debug("ExportPopup::setup() succeeded");
            ret->autorelease();
            return ret;
        }
        geode::log::error("ExportPopup::init() failed");
        delete ret;
        return nullptr;
    }

    void setExportPath(std::string const& path) {
        m_selectedPath = path;
    }

protected:
    std::vector<CCMenuItemSpriteExtra*> m_presetButtons;

    int m_presetW = 1920;
    int m_presetH = 1080;

    void loadSavedPreset() {
        m_presetW = Mod::get()->getSavedValue<int>("export-preset-w", 1920);
        m_presetH = Mod::get()->getSavedValue<int>("export-preset-h", 1080);
    }

    std::string m_selectedPath;

    bool setup() {
        geode::log::debug("ExportPopup::setup() called");
        this->setTitle("Export Objects");

        auto winSize = CCDirector::get()->getWinSize();
        auto* layer = m_mainLayer;
        geode::log::debug("m_mainLayer: {}", fmt::ptr(layer));

        loadSavedPreset();

        struct Preset { const char* label; int w; int h; };
        constexpr Preset presets[] = {
            {"720p",  1280,  720},
            {"1080p", 1920, 1080},
            {"1440p", 2560, 1440},
            {"4K",    3840, 2160},
        };

        auto* presetMenu = CCMenu::create();
        presetMenu->setLayout(
            RowLayout::create()
                ->setGap(6.f)
                ->setAxisAlignment(AxisAlignment::Center)
        );
        presetMenu->setContentSize({300.f, 36.f});
        presetMenu->setPosition({170.f, 185.f});

        for (auto& p : presets) {
            const char* texture = (p.w == m_presetW && p.h == m_presetH) ? "GJ_button_02.png" : "GJ_button_04.png";
            auto* btn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(p.label, 52, true, "bigFont.fnt", texture, 28.f, 0.6f),
                this,
                menu_selector(ExportPopup::onPreset)
            );
            btn->setTag(p.w);
            btn->setUserData((void*)(intptr_t)p.h);
            presetMenu->addChild(btn);
            m_presetButtons.push_back(btn);
        }

        presetMenu->updateLayout();
        layer->addChild(presetMenu);
        geode::log::debug("Added presetMenu with {} children", presetMenu->getChildrenCount());

        auto dimStr = fmt::format("{} x {}", m_presetW, m_presetH);
        m_dimLabel = CCLabelBMFont::create(dimStr.c_str(), "bigFont.fnt");
        m_dimLabel->setScale(0.55f);
        m_dimLabel->setPosition({170.f, 148.f});
        layer->addChild(m_dimLabel);

        auto* gearSprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        gearSprite->setScale(0.7f);
        auto* gearBtn = CCMenuItemSpriteExtra::create(
            gearSprite,
            this,
            menu_selector(ExportPopup::onSettings)
        );
        auto* gearMenu = CCMenu::create();
        gearMenu->setPosition({25.f, 25.f});
        gearMenu->addChild(gearBtn);
        layer->addChild(gearMenu);

        auto* exportMenu = CCMenu::create();
        exportMenu->setPosition({170.f, 62.f});

        int savedFmt = Mod::get()->getSavedValue<int>("export-format", static_cast<int>(ExportFormat::PNG));
        if (savedFmt < 0 || savedFmt >= static_cast<int>(ExportFormat::COUNT)) savedFmt = static_cast<int>(ExportFormat::PNG);
        const char* fmtName = s_formatNames[savedFmt];
        std::string exportLabel = fmt::format("Export {}", fmtName);

        auto* exportBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(exportLabel.c_str(), 120, true, "bigFont.fnt", "GJ_button_01.png", 40.f, 0.8f),
            this,
            menu_selector(ExportPopup::onExport)
        );
        exportMenu->addChild(exportBtn);
        layer->addChild(exportMenu);


        auto* editor = LevelEditorLayer::get();
        int count = 0;
        if (editor) {
            auto sel = editor->m_editorUI->getSelectedObjects();
            count = sel ? (int)sel->count() : 0;
        }
        auto countStr = fmt::format("{} object{} selected", count, count == 1 ? "" : "s");
        auto* countLabel = CCLabelBMFont::create(countStr.c_str(), "chatFont.fnt");
        countLabel->setScale(0.55f);
        countLabel->setColor({180, 180, 180});
        countLabel->setPosition({170.f, 28.f});
        layer->addChild(countLabel);

#ifdef GEODE_IS_ANDROID
        auto* androidInfo = CCLabelBMFont::create("Images saved to Pictures/Exports", "chatFont.fnt");
        androidInfo->setScale(0.4f);
        androidInfo->setColor({150, 200, 255});
        androidInfo->setPosition({170.f, 12.f});
        layer->addChild(androidInfo);
#endif

        return true;
    }

    void onPreset(CCObject* sender) {
        m_presetW = sender->getTag();
        m_presetH = (int)(intptr_t)static_cast<CCNode*>(sender)->getUserData();

        Mod::get()->setSavedValue<int>("export-preset-w", m_presetW);
        Mod::get()->setSavedValue<int>("export-preset-h", m_presetH);

        for (auto* btn : m_presetButtons) {
            int btnW = btn->getTag();
            int btnH = (int)(intptr_t)btn->getUserData();
            const char* texture = (btnW == m_presetW && btnH == m_presetH) ? "GJ_button_02.png" : "GJ_button_04.png";

            const char* label = nullptr;
            if (btnW == 1280 && btnH == 720) label = "720p";
            else if (btnW == 1920 && btnH == 1080) label = "1080p";
            else if (btnW == 2560 && btnH == 1440) label = "1440p";
            else if (btnW == 3840 && btnH == 2160) label = "4K";

            if (label) {
                auto* newSprite = ButtonSprite::create(label, 52, true, "bigFont.fnt", texture, 28.f, 0.6f);
                btn->setNormalImage(newSprite);
                btn->setSelectedImage(newSprite);
            }
        }

        auto str = fmt::format("{} x {}", m_presetW, m_presetH);
        m_dimLabel->setString(str.c_str());
    }

    void onSettings(CCObject*) {
        ExportSettingsPopup::create()->show();
    }


    void onExport(CCObject*) {
        int w = m_presetW;
        int h = m_presetH;

        if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
            FLAlertLayer::create("Invalid Size", "Width and height must be between 1 and 16384.", "OK")->show();
            return;
        }

        bool transparent = Mod::get()->getSavedValue<bool>("export-transparent-bg", false);
        bool cropToVisible = Mod::get()->getSavedValue<bool>("export-crop-visible", false);

        ExportFormat format = static_cast<ExportFormat>(Mod::get()->getSavedValue<int>("export-format", static_cast<int>(ExportFormat::PNG)));
        ExportCompression compression = static_cast<ExportCompression>(Mod::get()->getSavedValue<int>("export-compression", static_cast<int>(ExportCompression::Lossless)));
        int quality = Mod::get()->getSavedValue<int>("export-quality", 90);

        std::string fullPath = m_selectedPath;
        if (fullPath.empty()) {
            std::string folder = Mod::get()->getSettingValue<std::string>("export-path");
#ifdef GEODE_IS_ANDROID
            folder = "/storage/emulated/0/Pictures/Exports";
#else
            if (folder.empty()) {
                folder = (std::filesystem::path(CCFileUtils::get()->getWritablePath())).string();
            }
#endif
            auto now = std::chrono::system_clock::now();
            auto t   = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            char timebuf[32];
            std::strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", &tm);
            std::string filename = fmt::format("gd_export_{}.{}", timebuf, formatExtension(format));
            fullPath = (std::filesystem::path(folder) / filename).string();
        }

        this->setVisible(false);

        bool ok = exportSelectedObjects(w, h, transparent, cropToVisible, format, compression, quality, fullPath);
        // fullPath may be updated to a supported extension if the chosen format is not yet implemented

        this->setVisible(true);

        if (ok) {
            auto msg = fmt::format("Saved to:\n{}", fullPath);
            FLAlertLayer::create("Exported!", msg.c_str(), "OK")->show();
        } else {
            FLAlertLayer::create("Export Failed", "Nothing selected or render error.", "OK")->show();
        }

        this->onClose(nullptr);
    }

private:
    CCLabelBMFont* m_dimLabel = nullptr;
};
