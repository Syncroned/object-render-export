#include "utils/Paths.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

#include <chrono>
#include <string>

#ifdef _WIN32
#include <windows.h>

#include <comdef.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

using namespace geode::prelude;

namespace paths {

namespace {

#ifdef _WIN32
std::string pwszToUtf8(PWSTR wstr) {
    if (!wstr) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), size, nullptr, nullptr);
    out.pop_back();
    return out;
}
#endif

}

void removeQuiet(std::filesystem::path const& p) {
    std::error_code ec;
    std::filesystem::remove(p, ec);
}

bool ensureParentDir(std::filesystem::path const& p) {
    auto parent = p.parent_path();
    if (parent.empty()) return true;
    auto res = geode::utils::file::createDirectoryAll(parent);
    if (!res) {
        geode::log::error("Could not create directory {}: {}", parent.string(), res.unwrapErr());
        return false;
    }
    return true;
}

std::filesystem::path platformDefaultExportDir() {
#ifdef _WIN32
    return picturesFolder();
#else
    return std::filesystem::path("/storage/emulated/0/Download/Exports");
#endif
}

std::filesystem::path resolveExportDir() {
    auto configured = Mod::get()->getSettingValue<std::filesystem::path>("export-path");
    if (!configured.empty()) return configured;
    return platformDefaultExportDir();
}

std::string makeExportFileName(std::string_view extension) {
    auto now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return fmt::format("object_export-{}.{}", millis, extension);
}

#ifdef _WIN32
std::string showFolderPickerDialog() {
    std::string result;

    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool shouldUninitialize = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        geode::log::error("CoInitializeEx failed for the folder picker: {:#x}", static_cast<unsigned>(init));
        return result;
    }

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
                                  reinterpret_cast<void**>(&dialog));

    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options))) {
            dialog->SetOptions(options | FOS_PICKFOLDERS);
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR folderPath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &folderPath))) {
                    result = pwszToUtf8(folderPath);
                    CoTaskMemFree(folderPath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (shouldUninitialize) CoUninitialize();
    return result;
}

std::filesystem::path picturesFolder() {
    std::filesystem::path result;
    PWSTR picsPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &picsPath))) {
        result = std::filesystem::path(pwszToUtf8(picsPath)).make_preferred();
        CoTaskMemFree(picsPath);
    }
    return result;
}
#endif

}
