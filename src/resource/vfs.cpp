// vfs.cpp — .pak 资源包读写实现（格式见 vfs.h 头注释）
#include "resource/vfs.h"
#include "core/log.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mg {
namespace vfs {

namespace fs = std::filesystem;

namespace {

constexpr char kMagic[4] = {'M', 'G', 'P', 'K'};
constexpr uint32_t kVersion = 1;
constexpr uint64_t kHeaderSize = 4 + 4 + 4 + 8; // magic + version + count + index_offset

struct PakEntry {
    uint64_t offset = 0;
    uint64_t size = 0;
};

// 挂载状态（单 pak，重复挂载替换）
std::string g_pak_path;
std::unordered_map<std::string, PakEntry> g_index; // 规范化路径 → 位置

#ifdef _WIN32
std::wstring Utf8ToWide(const char* utf8) {
    int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring w(len - 1, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w.data(), len);
    return w;
}
#endif

// 磁盘读取（pak 未命中时的回退路径）
bool ReadFileFromDisk(const std::string& path, std::vector<uint8_t>& out) {
#ifdef _WIN32
    std::wstring wpath = Utf8ToWide(path.c_str());
    HANDLE h = ::CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size_li;
    if (!::GetFileSizeEx(h, &size_li)) { ::CloseHandle(h); return false; }
    size_t size = static_cast<size_t>(size_li.QuadPart);
    out.resize(size);
    DWORD read = 0;
    BOOL ok = size == 0 || ::ReadFile(h, out.data(), static_cast<DWORD>(size), &read, nullptr);
    ::CloseHandle(h);
    if (!ok || read != size) { out.clear(); return false; }
    return true;
#else
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (!out.empty() && !file.read(reinterpret_cast<char*>(out.data()), size)) {
        out.clear();
        return false;
    }
    return true;
#endif
}

} // namespace

std::string NormalizePath(const std::string& path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    while (p.rfind("./", 0) == 0) p.erase(0, 2);
    return p;
}

bool MountPak(const std::string& pak_path) {
    UnmountPak();

    std::ifstream f(pak_path, std::ios::binary);
    if (!f) {
        MG_LOG_ERROR("vfs: cannot open pak: " << pak_path);
        return false;
    }

    char magic[4];
    uint32_t version = 0, count = 0;
    uint64_t index_offset = 0;
    f.read(magic, 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    f.read(reinterpret_cast<char*>(&count), 4);
    f.read(reinterpret_cast<char*>(&index_offset), 8);
    if (!f || std::memcmp(magic, kMagic, 4) != 0 || version != kVersion) {
        MG_LOG_ERROR("vfs: bad pak magic/version: " << pak_path);
        return false;
    }
    if (count > (1u << 20)) { //  sanity：索引条目数上限
        MG_LOG_ERROR("vfs: pak entry count absurd: " << count);
        return false;
    }

    f.seekg(static_cast<std::streamoff>(index_offset), std::ios::beg);
    std::unordered_map<std::string, PakEntry> index;
    index.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t path_len = 0;
        f.read(reinterpret_cast<char*>(&path_len), 2);
        if (!f || path_len == 0) break;
        std::string path(path_len, '\0');
        f.read(path.data(), path_len);
        PakEntry e;
        f.read(reinterpret_cast<char*>(&e.offset), 8);
        f.read(reinterpret_cast<char*>(&e.size), 8);
        if (!f) break;
        index.emplace(std::move(path), e);
    }
    if (!f || index.size() != count) {
        MG_LOG_ERROR("vfs: corrupted pak index: " << pak_path);
        return false;
    }

    g_pak_path = pak_path;
    g_index = std::move(index);
    MG_LOG_INFO("vfs: mounted " << pak_path << " (" << g_index.size() << " entries)");
    return true;
}

void UnmountPak() {
    g_pak_path.clear();
    g_index.clear();
}

bool IsPakMounted() {
    return !g_pak_path.empty();
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    if (IsPakMounted()) {
        auto it = g_index.find(NormalizePath(path));
        if (it != g_index.end()) {
            std::ifstream f(g_pak_path, std::ios::binary);
            if (!f) return false;
            f.seekg(static_cast<std::streamoff>(it->second.offset), std::ios::beg);
            out.resize(static_cast<size_t>(it->second.size));
            if (!out.empty() &&
                !f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
                out.clear();
                return false;
            }
            return true;
        }
    }
    return ReadFileFromDisk(path, out);
}

bool Exists(const std::string& path) {
    if (IsPakMounted() && g_index.contains(NormalizePath(path))) return true;
    std::error_code ec;
    return fs::exists(path, ec);
}

bool WritePak(const std::string& root_dir, const std::vector<std::string>& rel_paths,
              const std::string& out_pak) {
    std::ofstream f(out_pak, std::ios::binary | std::ios::trunc);
    if (!f) {
        MG_LOG_ERROR("vfs: cannot create pak: " << out_pak);
        return false;
    }

    // 头部占位（最后回填 index_offset / count）
    char header[kHeaderSize] = {};
    std::memcpy(header, kMagic, 4);
    std::memcpy(header + 4, &kVersion, 4);
    f.write(header, sizeof(header));

    std::vector<std::pair<std::string, PakEntry>> entries;
    entries.reserve(rel_paths.size());
    std::vector<uint8_t> bytes;
    for (const auto& rel : rel_paths) {
        std::string norm = NormalizePath(rel);
        if (norm.empty()) continue;
        if (!ReadFileFromDisk(root_dir + "/" + rel, bytes)) {
            MG_LOG_ERROR("vfs: pack source missing: " << rel);
            return false;
        }
        PakEntry e;
        e.offset = static_cast<uint64_t>(f.tellp());
        e.size = bytes.size();
        if (!bytes.empty())
            f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        entries.emplace_back(std::move(norm), e);
    }

    uint64_t index_offset = static_cast<uint64_t>(f.tellp());
    for (const auto& [path, e] : entries) {
        uint16_t path_len = static_cast<uint16_t>(path.size());
        f.write(reinterpret_cast<const char*>(&path_len), 2);
        f.write(path.data(), path_len);
        f.write(reinterpret_cast<const char*>(&e.offset), 8);
        f.write(reinterpret_cast<const char*>(&e.size), 8);
    }

    // 回填头部
    uint32_t count = static_cast<uint32_t>(entries.size());
    f.seekp(8, std::ios::beg);
    f.write(reinterpret_cast<const char*>(&count), 4);
    f.write(reinterpret_cast<const char*>(&index_offset), 8);
    f.flush();
    if (!f) {
        MG_LOG_ERROR("vfs: failed writing pak: " << out_pak);
        return false;
    }
    MG_LOG_INFO("vfs: wrote " << out_pak << " (" << count << " entries)");
    return true;
}

} // namespace vfs
} // namespace mg
