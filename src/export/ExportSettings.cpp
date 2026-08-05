#include "export/ExportSettings.hpp"

std::pair<ExportSettings, std::vector<std::string>> reconcileExportSettings(ExportSettings settings) {
    std::vector<std::string> messages;
    bool lossy = formatSupportsLossy(settings.format);
    bool lossless = formatSupportsLossless(settings.format);

    if (settings.compression == ExportCompression::Lossy && !lossy) {
        if (lossless) {
            settings.compression = ExportCompression::Lossless;
            messages.push_back("This format does not support lossy compression. Switched to lossless.");
        } else {
            settings.compression = ExportCompression::Lossless;
        }
    } else if (settings.compression == ExportCompression::Lossless && !lossless) {
        if (lossy) {
            settings.compression = ExportCompression::Lossy;
            messages.push_back("This format does not support lossless compression. Switched to lossy.");
        } else {
            settings.compression = ExportCompression::Lossy;
        }
    }

    if (settings.transparent && !formatSupportsTransparency(settings.format)) {
        settings.transparent = false;
        messages.push_back("This format does not support transparency. Transparent BG turned off.");
    }

    if (settings.transparent && settings.compression == ExportCompression::Lossy) {
        settings.compression = ExportCompression::Lossless;
        messages.push_back("This format cannot combine lossy compression with transparency. Switched to lossless.");
    }

    return {settings, messages};
}
