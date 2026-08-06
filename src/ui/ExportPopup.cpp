#include "ui/ExportPopup.hpp"
#include "ui/ExportSettingsPopup.hpp"
#include "ui/PopupWidgets.hpp"
#include "export/ExportSettings.hpp"
#include "export/ExportPipeline.hpp"
#include "utils/Paths.hpp"

using namespace geode::prelude;

ExportPopup* ExportPopup::create() {
    auto* ret = new ExportPopup();
    if (ret && ret->init(340.f, 240.f)) {
        if (!ret->setup()) {
            geode::log::error("ExportPopup::setup() failed");
            delete ret;
            return nullptr;
        }
        ret->autorelease();
        return ret;
    }
    geode::log::error("ExportPopup::init() failed");
    delete ret;
    return nullptr;
}

void ExportPopup::setExportPath(std::string const& path) {
    m_selectedPath = path;
}

void ExportPopup::loadSavedPreset() {
    m_presetW = Mod::get()->getSavedValue<int>("export-preset-w", 1920);
    m_presetH = Mod::get()->getSavedValue<int>("export-preset-h", 1080);
}

bool ExportPopup::isSelected(Preset const& p) const {
    return p.w == m_presetW && p.h == m_presetH;
}

ButtonSprite* ExportPopup::presetSprite(Preset const& p, bool selected) {
    return ButtonSprite::create(p.label, 52, true, "bigFont.fnt",
        selected ? "GJ_button_02.png" : "GJ_button_04.png", 28.f, 0.6f);
}

bool ExportPopup::setup() {
    this->setTitle("Export Objects");
    auto* layer = m_mainLayer;

    loadSavedPreset();

    auto* topMenu = CCMenu::create();
    topMenu->setLayout(
        RowLayout::create()
            ->setGap(6.f)
            ->setAxisAlignment(AxisAlignment::Center)
    );
    topMenu->setContentSize({300.f, 36.f});
    topMenu->setPosition({170.f, 195.f});

    auto* bottomMenu = CCMenu::create();
    bottomMenu->setLayout(
        RowLayout::create()
            ->setGap(6.f)
            ->setAxisAlignment(AxisAlignment::Center)
    );
    bottomMenu->setContentSize({140.f, 36.f});
    bottomMenu->setPosition({170.f, 155.f});

    for (int i = 0; i < static_cast<int>(std::size(c_presets)); ++i) {
        auto& p = c_presets[i];
        auto* btn = CCMenuItemSpriteExtra::create(
            presetSprite(p, isSelected(p)),
            this,
            menu_selector(ExportPopup::onPreset)
        );
        btn->setTag(i);
        (i < 4 ? topMenu : bottomMenu)->addChild(btn);
        m_presetButtons.push_back(btn);
    }

    topMenu->updateLayout();
    bottomMenu->updateLayout();
    layer->addChild(topMenu);
    layer->addChild(bottomMenu);

    auto dimStr = fmt::format("{} x {}", m_presetW, m_presetH);
    m_dimLabel = CCLabelBMFont::create(dimStr.c_str(), "bigFont.fnt");
    m_dimLabel->setScale(0.55f);
    m_dimLabel->setPosition({170.f, 118.f});
    layer->addChild(m_dimLabel);

    auto* gearBtn = makeIconButton("GJ_optionsBtn_001.png", 0.7f, this, menu_selector(ExportPopup::onSettings));
    auto* gearMenu = CCMenu::create();
    gearMenu->setPosition({25.f, 25.f});
    gearMenu->addChild(gearBtn);
    layer->addChild(gearMenu);

    auto* exportMenu = CCMenu::create();
    exportMenu->setPosition({170.f, 62.f});

    auto settings = loadExportSettings();
    std::string exportLabel = fmt::format("Export {}", formatName(settings.format));

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
    auto destStr = fmt::format("Images saved to {}", paths::resolveExportDir().string());
    auto* androidInfo = CCLabelBMFont::create(destStr.c_str(), "chatFont.fnt");
    androidInfo->setScale(0.4f);
    androidInfo->setColor({150, 200, 255});
    androidInfo->setPosition({170.f, 12.f});
    layer->addChild(androidInfo);
#endif

    return true;
}

void ExportPopup::onPreset(CCObject* sender) {
    auto& picked = c_presets[static_cast<CCNode*>(sender)->getTag()];
    m_presetW = picked.w;
    m_presetH = picked.h;

    if (m_presetW == 15360 && m_presetH == 8640) {
        FLAlertLayer::create("Warning", "Exporting at 16K may cause Geometry Dash to freeze for a while.", "OK")->show();
    }

    Mod::get()->setSavedValue<int>("export-preset-w", m_presetW);
    Mod::get()->setSavedValue<int>("export-preset-h", m_presetH);

    for (auto* btn : m_presetButtons) {
        auto& p = c_presets[btn->getTag()];
        auto* newSprite = presetSprite(p, isSelected(p));
        btn->setNormalImage(newSprite);
        btn->setSelectedImage(newSprite);
    }

    m_dimLabel->setString(fmt::format("{} x {}", m_presetW, m_presetH).c_str());
}

void ExportPopup::onSettings(CCObject*) {
    ExportSettingsPopup::create()->show();
}

void ExportPopup::onExport(CCObject*) {
    int w = m_presetW;
    int h = m_presetH;

    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        FLAlertLayer::create("Invalid Size", "Width and height must be between 1 and 16384.", "OK")->show();
        return;
    }

    auto settings = loadExportSettings();

    std::string fullPath = m_selectedPath;
    if (fullPath.empty()) {
        fullPath = (paths::resolveExportDir() /
                    paths::makeExportFileName(formatExtension(settings.format))).string();
    }

    this->setVisible(false);

    geode::Ref<LoadingCircle> circle = LoadingCircle::create();
    circle->setFade(false);
    if (auto* editor = LevelEditorLayer::get()) {
        circle->setParentLayer(editor);
    }
    circle->show();

    geode::Ref<ExportPopup> self = this;
    Loader::get()->queueInMainThread([self, circle, settings, w, h, fullPath]() mutable {
        bool ok = exportSelectedObjects(w, h, settings.transparent, settings.crop, settings.format,
            settings.compression, settings.quality, fullPath);

        circle->fadeAndRemove();
        self->setVisible(true);

        if (ok) {
            FLAlertLayer::create("Exported!", fmt::format("Saved to:\n{}", fullPath).c_str(), "OK")->show();
        } else {
            FLAlertLayer::create("Export Failed", "Nothing selected or render error.", "OK")->show();
        }

        self->onClose(nullptr);
    });
}
