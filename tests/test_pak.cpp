// test_pak.cpp — .pak 资源包读写 round-trip / 路径规范化 / 磁盘回退 / 坏包处理
#include "test_framework.h"
#include "resource/vfs.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// 每个用例独立的临时目录（进程结束自动清理由调用者保证简单：用 unique 子目录）
struct TempDir {
    fs::path dir;
    TempDir() {
        dir = fs::temp_directory_path() / ("mg_pak_test_" + std::to_string(
            std::hash<std::string>{}(std::to_string(rand()))));
        fs::create_directories(dir);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(dir, ec); }
};

void WriteFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << content;
}

} // namespace

// ---- 路径规范化 ----

MG_TEST(pak_normalize_path) {
    MG_EXPECT_EQ(mg::vfs::NormalizePath("./assets/x.png"), std::string("assets/x.png"));
    MG_EXPECT_EQ(mg::vfs::NormalizePath("assets\\x.png"), std::string("assets/x.png"));
    MG_EXPECT_EQ(mg::vfs::NormalizePath(".\\assets\\x.png"), std::string("assets/x.png"));
    MG_EXPECT_EQ(mg::vfs::NormalizePath("assets/x.png"), std::string("assets/x.png"));
}

// ---- round-trip：写包 → 挂载 → 读回 ----

MG_TEST(pak_roundtrip) {
    TempDir tmp;
    WriteFile(tmp.dir / "project.json", R"({"name":"T"})");
    WriteFile(tmp.dir / "maps/Map001.json", R"({"entities":[]})");
    WriteFile(tmp.dir / "assets/x.bin", std::string("\x00\x01\x02\xff", 4));

    fs::path pak = tmp.dir / "data.pak";
    bool ok = mg::vfs::WritePak(tmp.dir.string(),
                                {"project.json", "maps/Map001.json", "assets/x.bin"},
                                pak.string());
    MG_EXPECT(ok);
    MG_EXPECT(fs::exists(pak));

    MG_EXPECT(mg::vfs::MountPak(pak.string()));
    MG_EXPECT(mg::vfs::IsPakMounted());

    std::vector<uint8_t> out;
    MG_EXPECT(mg::vfs::ReadFile("project.json", out));
    MG_EXPECT_EQ(std::string(out.begin(), out.end()), std::string(R"({"name":"T"})"));

    // 规范化路径命中：./ 与反斜杠形态
    MG_EXPECT(mg::vfs::ReadFile("./maps/Map001.json", out));
    MG_EXPECT_EQ(std::string(out.begin(), out.end()), std::string(R"({"entities":[]})"));
    MG_EXPECT(mg::vfs::ReadFile("assets\\x.bin", out));
    MG_EXPECT_EQ(out.size(), (size_t)4);
    MG_EXPECT_EQ(out[3], (uint8_t)0xff);

    MG_EXPECT(mg::vfs::Exists("maps/Map001.json"));
    mg::vfs::UnmountPak();
    MG_EXPECT(!mg::vfs::IsPakMounted());
}

// ---- 磁盘回退：pak 未命中的路径落盘 ----

MG_TEST(pak_disk_fallback) {
    TempDir tmp;
    WriteFile(tmp.dir / "loose.txt", "loose-data");
    WriteFile(tmp.dir / "packed.txt", "packed-data");

    fs::path pak = tmp.dir / "data.pak";
    MG_EXPECT(mg::vfs::WritePak(tmp.dir.string(), {"packed.txt"}, pak.string()));
    MG_EXPECT(mg::vfs::MountPak(pak.string()));

    std::vector<uint8_t> out;
    // pak 内命中
    MG_EXPECT(mg::vfs::ReadFile("packed.txt", out));
    MG_EXPECT_EQ(std::string(out.begin(), out.end()), std::string("packed-data"));
    // pak 未命中 → 落盘（用绝对路径绕过 cwd）
    std::string loose_abs = (tmp.dir / "loose.txt").generic_string();
    MG_EXPECT(mg::vfs::ReadFile(loose_abs, out));
    MG_EXPECT_EQ(std::string(out.begin(), out.end()), std::string("loose-data"));
    // 两边都不存在
    MG_EXPECT(!mg::vfs::Exists("no_such_file.txt"));

    mg::vfs::UnmountPak();
}

// ---- 坏包：错误 magic 挂载失败 ----

MG_TEST(pak_bad_magic_rejected) {
    TempDir tmp;
    WriteFile(tmp.dir / "bad.pak", "NOTAPAK-not-a-pak-at-all");
    MG_EXPECT(!mg::vfs::MountPak((tmp.dir / "bad.pak").string()));
    MG_EXPECT(!mg::vfs::IsPakMounted());
    // 不存在的文件
    MG_EXPECT(!mg::vfs::MountPak((tmp.dir / "missing.pak").string()));
}
