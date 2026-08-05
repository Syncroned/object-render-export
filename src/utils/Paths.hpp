#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace paths {

void removeQuiet(std::filesystem::path const& p);

bool ensureParentDir(std::filesystem::path const& p);

std::filesystem::path platformDefaultExportDir();

std::filesystem::path resolveExportDir();

std::string makeExportFileName(std::string_view extension);

#ifdef _WIN32
std::string showFolderPickerDialog();

std::filesystem::path picturesFolder();
#endif

}
