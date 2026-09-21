#include "file_dialog.h"
#include <algorithm>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), result.data(), size, nullptr, nullptr);
    return result;
}
#endif

namespace mg {
namespace editor {

std::vector<std::string> OpenFileDialogMulti(const char* filter_name, const char* filter_ext) {
    std::vector<std::string> result;
#ifdef _WIN32
    OPENFILENAMEW ofn = {};
    wchar_t buf[4096] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 4096;
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    std::string filter_str = std::string(filter_name) + '\0' + filter_ext + '\0' + '\0';
    std::wstring wfilter;
    int wsize = MultiByteToWideChar(CP_UTF8, 0, filter_str.c_str(), (int)filter_str.size(), nullptr, 0);
    wfilter.resize(wsize);
    MultiByteToWideChar(CP_UTF8, 0, filter_str.c_str(), (int)filter_str.size(), wfilter.data(), wsize);
    ofn.lpstrFilter = wfilter.c_str();

    if (GetOpenFileNameW(&ofn)) {
        std::wstring dir = buf;
        wchar_t* p = buf + dir.length() + 1;
        if (*p == L'\0') {
            result.push_back(WideToUtf8(dir));
        } else {
            while (*p) {
                std::wstring file = p;
                result.push_back(WideToUtf8(dir + L"\\" + file));
                p += file.length() + 1;
            }
        }
    }
#endif
    return result;
}

std::string OpenFolderDialog(const char* title) {
    std::string result;
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    std::wstring wtitle;
    if (wlen > 0) {
        wtitle.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, title, -1, &wtitle[0], wlen);
    }

    BROWSEINFOW bi = {};
    bi.lpszTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    wchar_t display_name[MAX_PATH];
    bi.pszDisplayName = display_name;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t wpath[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, wpath)) {
            CoTaskMemFree(pidl);
            result = WideToUtf8(wpath);
            std::replace(result.begin(), result.end(), '\\', '/');
            return result;
        }
        CoTaskMemFree(pidl);
    }
#endif
    return result;
}

} // namespace editor
} // namespace mg
