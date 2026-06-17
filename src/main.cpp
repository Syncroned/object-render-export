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

bool exportSelectedObjects(int width, int height, bool transparentBg, std::string const& path) {
    auto* lel = LevelEditorLayer::get();
    if (!lel) return false;

    auto* ui = lel->m_editorUI;
    CCArray* selected = ui->getSelectedObjects();
    if (!selected || selected->count() == 0) return false;

    std::ostringstream debugLog;
    auto logLine = [&](std::string const& line) {
        debugLog << line << "\n";
        geode::log::debug("{}", line);
    };

    logLine(fmt::format("[export] Called with width={} height={} transparentBg={}", width, height, transparentBg));

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

        if (data.shouldBlendBase || data.shouldBlendDetail) {
            logLine(fmt::format(
                "[export] BLEND obj {} baseChan={} (blend={}) detailChan={} (blend={}) "
                "=> base={} detail={}",
                obj->m_objectID, baseChan, channelBlends(baseChan), detailChan,
                channelBlends(detailChan), data.shouldBlendBase, data.shouldBlendDetail));
        }

        logLine(fmt::format(
            "[export] ZORDER obj {} | m_zLayer={} m_zOrder={} | getObjZLayer={} getObjZOrder={} "
            "| defZLayer={} defZOrder={} zFixed={} | classType={} objType={} deco={}/{} | uniqueID={}",
            obj->m_objectID, (int)obj->m_zLayer, obj->m_zOrder,
            (int)obj->getObjectZLayer(), obj->getObjectZOrder(),
            (int)obj->m_defaultZLayer, obj->m_defaultZOrder, obj->m_zFixedZLayer,
            (int)obj->m_classType, (int)obj->m_objectType,
            obj->m_isDecoration, obj->m_isDecoration2, obj->m_uniqueID));

        std::string keyStr;
        for (auto const& [z, idx] : data.drawKey) {
            keyStr += fmt::format("({},{})", z, idx);
        }
        logLine(fmt::format("[export] DRAWKEY obj {} => {}", obj->m_objectID, keyStr));

        originalData.push_back(data);
    }

    std::stable_sort(originalData.begin(), originalData.end(),
        [](ObjectData const& a, ObjectData const& b) {
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

    struct BlendSprite { CCSprite* sprite; ccBlendFunc func; bool blend; };
    std::vector<BlendSprite> renderSprites;

    const ccBlendFunc additive = {GL_SRC_ALPHA, GL_ONE};

    auto collectSprites = [&](CCNode* root, std::vector<CCNode*> const& skips,
                              std::vector<CCSprite*>& out) {
        std::function<void(CCNode*)> rec = [&](CCNode* node) {
            if (!node) return;
            for (auto* sk : skips) {
                if (node == sk) return;
            }
            if (auto* spr = geode::cast::typeinfo_cast<CCSprite*>(node)) {
                out.push_back(spr);
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

            std::vector<CCSprite*> toBlend;
            if (data.shouldBlendBase) {
                collectSprites(gameObject,
                    {gameObject->m_colorSprite, gameObject->m_glowSprite}, toBlend);
            }
            if (data.shouldBlendDetail && gameObject->m_colorSprite) {
                collectSprites(gameObject->m_colorSprite, {}, toBlend);
            }
            for (auto* s : toBlend) {
                renderSprites.push_back({s, additive, true});
            }

            visibleIndex++;
        }
    }

    int detachedCount = 0;
    int blendCount = 0;
    for (auto const& rs : renderSprites) {
        if (!rs.blend) continue;
        blendCount++;
        CCSprite* s = rs.sprite;
        CCSpriteBatchNode* bn = s->getBatchNode();
        CCNode* parent = s->getParent();
        logLine(fmt::format("[export] blend sprite parent={} batchNode={}",
            fmt::ptr(parent), fmt::ptr(bn)));

        if (!bn || !parent) {
            s->setBlendFunc(rs.func);
            continue;
        }

        int z = parent->getZOrder();
        CCPoint worldPos = parent->convertToWorldSpace(s->getPosition());
        CCPoint localPos = sprite->convertToNodeSpace(worldPos);

        s->retain();
        s->removeFromParent();
        s->setPosition(localPos);
        s->setBlendFunc(rs.func);
        sprite->addChild(s, z);
        s->release();
        detachedCount++;
    }
    logLine(fmt::format("[export] blendCount={} detachedCount={}", blendCount, detachedCount));

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
    bool saved = false;
    if (img) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        saved = img->saveToFile(path.c_str(), false);
        img->release();
    }

    rt->release();

    if (Mod::get()->getSettingValue<bool>("enable-logs")) {
        auto logDir = Mod::get()->getSaveDir() / "logs";
        std::filesystem::create_directories(logDir);

        auto logPath = logDir / ("export_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + ".log");
        std::ofstream out(logPath);
        if (out) {
            out << debugLog.str();
        }
    }

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
