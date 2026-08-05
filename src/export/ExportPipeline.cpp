#include "export/ExportPipeline.hpp"

#include "export/ObjectCollection.hpp"
#include "export/Renderer.hpp"
#include "export/SceneBuilder.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;
using namespace cocos2d;

bool exportSelectedObjects(int width, int height, bool transparentBg, bool cropToVisible,
    ExportFormat format, ExportCompression compression, int quality, std::string& path) {
    auto* lel = LevelEditorLayer::get();
    if (!lel) return false;

    auto* ui = lel->m_editorUI;
    CCArray* selected = ui->getSelectedObjects();
    if (!selected || selected->count() == 0) return false;

    auto const& ids22 = get22ObjectIDs();
    auto [originalData, exportWorldBounds] = collectObjectData(selected, lel, ids22);

    auto settings = loadExportSettings();

    std::vector<BorrowedNode> borrowedNodes;
    AdditiveParts additive;
    CCSprite* sprite = buildExportSprite(originalData, lel, borrowedNodes, additive, settings.includeGradients);
    BorrowGuard borrowGuard{borrowedNodes};

    ExportRequest request;
    request.width = width;
    request.height = height;
    request.transparentBg = transparentBg;
    request.cropToVisible = cropToVisible;
    request.includeGradients = settings.includeGradients;
    request.includeShaders = settings.includeShaders;
    request.format = format;
    request.compression = compression;
    request.quality = quality;

    return renderAndSave(sprite, request, additive, exportWorldBounds, selected, lel, path);
}
