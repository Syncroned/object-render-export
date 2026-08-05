#pragma once
#include "export/ExportSettings.hpp"
#include <cocos2d.h>

namespace cocos2d { class CCImage; }

using cocos2d::CCImage;

CCImage* cropImage(CCImage* src, int cropX, int cropY, int cropW, int cropH);
bool saveJPEG(CCImage* img, int quality, std::string const& path);
bool saveGIF(CCImage* img, bool transparentBg, std::string const& path);
bool saveImage(CCImage* img, ExportFormat format, ExportCompression compression, int quality, bool transparentBg, std::string& path);
bool findVisibleBounds(CCImage* img, int& outX, int& outY, int& outW, int& outH);
