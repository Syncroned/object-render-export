#pragma once
#include "export/ExportSettings.hpp"
#include <string>

bool exportSelectedObjects(int width, int height, bool transparentBg, bool cropToVisible,
    ExportFormat format, ExportCompression compression, int quality, std::string& path);
