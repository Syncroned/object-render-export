#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <array>
#include <string>
#include <vector>

enum class ExportFormat {
    PNG,
    JPEG,
    GIF,
    COUNT
};

enum class ExportCompression {
    Lossy,
    Lossless,
    COUNT
};

struct FormatInfo {
    const char* name;
    const char* ext;
    bool supportsTransparency;
    bool supportsLossy;
    bool supportsLossless;
};

constexpr std::array<FormatInfo, static_cast<size_t>(ExportFormat::COUNT)> c_formatInfo = {{
    {"PNG", "png", true, false, true},
    {"JPEG", "jpg", false, true, false},
    {"GIF", "gif", true, false, true},
}};

constexpr FormatInfo const& formatInfo(ExportFormat fmt) {
    int idx = std::clamp(static_cast<int>(fmt), 0, static_cast<int>(ExportFormat::COUNT) - 1);
    return c_formatInfo[static_cast<size_t>(idx)];
}

inline const char* formatName(ExportFormat fmt) { return formatInfo(fmt).name; }
inline const char* formatExtension(ExportFormat fmt) { return formatInfo(fmt).ext; }
inline bool formatSupportsTransparency(ExportFormat fmt) { return formatInfo(fmt).supportsTransparency; }
inline bool formatSupportsLossy(ExportFormat fmt) { return formatInfo(fmt).supportsLossy; }
inline bool formatSupportsLossless(ExportFormat fmt) { return formatInfo(fmt).supportsLossless; }

constexpr std::array<const char*, static_cast<size_t>(ExportCompression::COUNT)> c_compressionNames = { "Lossy", "Lossless" };

inline const char* compressionName(ExportCompression comp) {
    int idx = std::clamp(static_cast<int>(comp), 0, static_cast<int>(ExportCompression::COUNT) - 1);
    return c_compressionNames[static_cast<size_t>(idx)];
}

struct ExportSettings {
    ExportFormat format = ExportFormat::PNG;
    ExportCompression compression = ExportCompression::Lossless;
    int quality = 90;
    bool transparent = false;
    bool crop = false;
    bool includeGradients = false;
    bool includeShaders = false;
};

template <>
struct matjson::Serialize<ExportSettings> {
    static geode::Result<ExportSettings> fromJson(matjson::Value const& v) {
        ExportSettings s;
        s.format = static_cast<ExportFormat>(std::clamp(
            v["format"].as<int>().unwrapOr(static_cast<int>(ExportFormat::PNG)),
            0, static_cast<int>(ExportFormat::COUNT) - 1));
        s.compression = static_cast<ExportCompression>(std::clamp(
            v["compression"].as<int>().unwrapOr(static_cast<int>(ExportCompression::Lossless)),
            0, static_cast<int>(ExportCompression::COUNT) - 1));
        s.quality = std::clamp(v["quality"].as<int>().unwrapOr(90), 0, 100);
        s.transparent = v["transparent"].as<bool>().unwrapOr(false);
        s.crop = v["crop"].as<bool>().unwrapOr(false);
        s.includeGradients = v["includeGradients"].as<bool>().unwrapOr(false);
        s.includeShaders = v["includeShaders"].as<bool>().unwrapOr(false);
        return geode::Ok(s);
    }

    static matjson::Value toJson(ExportSettings const& s) {
        return matjson::makeObject({
            {"format", static_cast<int>(s.format)},
            {"compression", static_cast<int>(s.compression)},
            {"quality", s.quality},
            {"transparent", s.transparent},
            {"crop", s.crop},
            {"includeGradients", s.includeGradients},
            {"includeShaders", s.includeShaders},
        });
    }
};

inline ExportSettings loadExportSettings() {
    return geode::Mod::get()->getSavedValue<ExportSettings>("export-settings", ExportSettings{});
}

inline void saveExportSettings(ExportSettings const& s) {
    geode::Mod::get()->setSavedValue<ExportSettings>("export-settings", s);
}

std::pair<ExportSettings, std::vector<std::string>> reconcileExportSettings(ExportSettings settings);
