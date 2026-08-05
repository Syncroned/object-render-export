#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>
#include <filesystem>
#include <string>
#include "ui/ExportPopup.hpp"
#include "utils/Paths.hpp"

using namespace geode::prelude;
using namespace cocos2d;

class ShutdownLogger {
public:
    ~ShutdownLogger() {
        geode::log::info("Object Render Export is shutting down. Goodbye!");
    }
};
static ShutdownLogger s_shutdownLogger;

$execute {
#ifdef _WIN32
    auto mod = Mod::get();
    if (mod->getSettingValue<std::filesystem::path>("export-path").empty()) {
        if (auto pics = paths::picturesFolder(); !pics.empty()) {
            mod->setSettingValue<std::filesystem::path>("export-path", pics);
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

        auto fileName = paths::makeExportFileName("png");

#ifdef _WIN32
        std::filesystem::path dir;
        if (Mod::get()->getSettingValue<bool>("direct-save")) {
            dir = paths::resolveExportDir();
        } else {
            auto picked = paths::showFolderPickerDialog();
            if (picked.empty()) {
                FLAlertLayer::create(
                    "Export Cancelled",
                    "No folder selected.\nExport cancelled.",
                    "OK"
                )->show();
                return;
            }
            dir = picked;
        }
#else
        std::filesystem::path dir = paths::resolveExportDir();
#endif

        auto filePath = dir / fileName;

        if (!paths::ensureParentDir(filePath)) {
            FLAlertLayer::create(
                "Export Failed",
                "Could not create the export folder.\nCheck the export path in the mod settings.",
                "OK"
            )->show();
            return;
        }

        if (auto* popup = ExportPopup::create()) {
            popup->setExportPath(geode::utils::string::pathToString(filePath));
            popup->show();
        }
    }
};
