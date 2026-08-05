#pragma once


#include "export/ExportSettings.hpp"
#include "export/SceneBuilder.hpp"

#include <Geode/Geode.hpp>
#include <cocos2d.h>

#include <string>

struct ExportRequest {
    int width = 0;
    int height = 0;
    bool transparentBg = false;
    bool cropToVisible = false;
    bool includeGradients = false;
    bool includeShaders = false;
    ExportFormat format = ExportFormat::PNG;
    ExportCompression compression = ExportCompression::Lossless;
    int quality = 90;
};

bool renderAndSave(cocos2d::CCSprite* sprite, ExportRequest const& request, AdditiveParts const& additive,
    cocos2d::CCRect const& worldBounds, cocos2d::CCArray* selected, LevelEditorLayer* lel, std::string& path);
