#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include "export/ExportSettings.hpp"

using namespace geode::prelude;

class ExportPopup : public geode::Popup {
public:
    static ExportPopup* create();
    void setExportPath(std::string const& path);

protected:
    struct Preset {
        const char* label;
        int w;
        int h;
    };

    static constexpr Preset c_presets[] = {
        {"720p", 1280, 720},
        {"1080p", 1920, 1080},
        {"1440p", 2560, 1440},
        {"4K", 3840, 2160},
    };

    std::vector<CCMenuItemSpriteExtra*> m_presetButtons;
    CCLabelBMFont* m_dimLabel = nullptr;
    std::string m_selectedPath;

    int m_presetW = 1920;
    int m_presetH = 1080;

    void loadSavedPreset();
    bool isSelected(Preset const& p) const;
    static ButtonSprite* presetSprite(Preset const& p, bool selected);
    bool setup();

    void onPreset(CCObject* sender);
    void onSettings(CCObject*);
    void onExport(CCObject*);
};
