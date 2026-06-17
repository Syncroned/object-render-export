#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

bool exportSelectedObjects(int width, int height, bool transparentBg, std::string const& path);

class ExportPopup : public geode::Popup {
public:
    static ExportPopup* create() {
        geode::log::debug("ExportPopup::create() called");
        auto* ret = new ExportPopup();
        geode::log::debug("ExportPopup allocated: {}", fmt::ptr(ret));
        if (ret && ret->init(340.f, 240.f)) {
            geode::log::debug("ExportPopup::init() succeeded");
            // Manually call setup() since Geode Popup isn't calling it
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
    CCMenuItemToggler* m_transparentToggle = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_presetButtons;

    // Selected preset dimensions
    int m_presetW = 1920;
    int m_presetH = 1080;

    // Selected file path from dialog
    std::string m_selectedPath;

    bool setup() {
        geode::log::debug("ExportPopup::setup() called");
        this->setTitle("Export Objects");

        auto winSize = CCDirector::get()->getWinSize();
        auto* layer = m_mainLayer;
        geode::log::debug("m_mainLayer: {}", fmt::ptr(layer));

        // ── Preset buttons ──────────────────────────────
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
            // Use GJ_button_02 for the initially selected preset (1080p), GJ_button_04 for others
            const char* texture = (p.w == 1920 && p.h == 1080) ? "GJ_button_02.png" : "GJ_button_04.png";
            auto* btn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(p.label, 52, true, "bigFont.fnt", texture, 28.f, 0.6f),
                this,
                menu_selector(ExportPopup::onPreset)
            );
            btn->setTag(p.w);            // store width in tag
            btn->setUserData((void*)(intptr_t)p.h); // store height in userdata
            presetMenu->addChild(btn);
            m_presetButtons.push_back(btn);
        }

        presetMenu->updateLayout();
        layer->addChild(presetMenu);
        geode::log::debug("Added presetMenu with {} children", presetMenu->getChildrenCount());

        // ── Dimension display ──────────
        m_dimLabel = CCLabelBMFont::create("1920 × 1080", "bigFont.fnt");
        m_dimLabel->setScale(0.55f);
        m_dimLabel->setPosition({170.f, 148.f});
        layer->addChild(m_dimLabel);

        // ── Transparent BG toggle ───────────────────────
        auto* toggleLabel = CCLabelBMFont::create("Transparent BG", "bigFont.fnt");
        toggleLabel->setScale(0.45f);
        toggleLabel->setPosition({130.f, 108.f});
        layer->addChild(toggleLabel);

        m_transparentToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(ExportPopup::onToggleBg), 0.7f
        );
        m_transparentToggle->toggle(false);

        auto* toggleMenu = CCMenu::create();
        toggleMenu->setPosition({220.f, 108.f});
        toggleMenu->addChild(m_transparentToggle);
        layer->addChild(toggleMenu);

        // ── Export button ────────────────────────────────
        auto* exportMenu = CCMenu::create();
        exportMenu->setPosition({170.f, 62.f});

        auto* exportBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Export PNG", 120, true, "bigFont.fnt", "GJ_button_01.png", 40.f, 0.8f),
            this,
            menu_selector(ExportPopup::onExport)
        );
        exportMenu->addChild(exportBtn);
        layer->addChild(exportMenu);

        // ── Clear logs button ────────────────────────────
        auto* clearLogsMenu = CCMenu::create();
        clearLogsMenu->setPosition({280.f, 28.f});

        auto* clearLogsBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Clear Logs", 80, true, "goldFont.fnt", "GJ_button_05.png", 20.f, 0.6f),
            this,
            menu_selector(ExportPopup::onClearLogs)
        );
        clearLogsMenu->addChild(clearLogsBtn);
        layer->addChild(clearLogsMenu);

        // ── Object count hint ────────────────────────────
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

        return true;
    }

    void onPreset(CCObject* sender) {
        m_presetW = sender->getTag();
        m_presetH = (int)(intptr_t)static_cast<CCNode*>(sender)->getUserData();

        // Update button textures - selected gets GJ_button_02, others get GJ_button_04
        for (auto* btn : m_presetButtons) {
            int btnW = btn->getTag();
            int btnH = (int)(intptr_t)btn->getUserData();
            const char* texture = (btnW == m_presetW && btnH == m_presetH) ? "GJ_button_02.png" : "GJ_button_04.png";

            // Get the preset label from the button's tag/userdata
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

        auto str = fmt::format("{} × {}", m_presetW, m_presetH);
        m_dimLabel->setString(str.c_str());
    }

    void onToggleBg(CCObject*) {
    }

    void onClearLogs(CCObject*) {
        auto logDir = Mod::get()->getSaveDir() / "logs";
        int deleted = 0;
        try {
            if (std::filesystem::exists(logDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
                    if (entry.path().extension() == ".log") {
                        std::filesystem::remove(entry.path());
                        deleted++;
                    }
                }
            }
        } catch (const std::exception& e) {
            geode::log::error("Failed to clear logs: {}", e.what());
        }

        auto msg = fmt::format("Cleared {} log file{}", deleted, deleted == 1 ? "" : "s");
        FLAlertLayer::create("Logs Cleared", msg.c_str(), "OK")->show();
    }

    void onExport(CCObject*) {
        int w = m_presetW;
        int h = m_presetH;

        if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
            FLAlertLayer::create("Invalid Size", "Width and height must be between 1 and 16384.", "OK")->show();
            return;
        }

        bool transparent = m_transparentToggle->isToggled();

        // Use the selected path from file dialog
        std::string fullPath = m_selectedPath;
        if (fullPath.empty()) {
            // Fallback to settings if dialog was cancelled
            std::string folder = Mod::get()->getSettingValue<std::string>("export-path");
            if (folder.empty()) {
                folder = (std::filesystem::path(CCFileUtils::get()->getWritablePath())).string();
            }
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
            std::string filename = fmt::format("gd_export_{}.png", timebuf);
            fullPath = (std::filesystem::path(folder) / filename).string();
        }

        this->setVisible(false);

        bool ok = exportSelectedObjects(w, h, transparent, fullPath);

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
