#pragma once
// vfs.h — 虚拟文件系统：.pak 资源包读取 + 透明磁盘回退。
// 运行时（game.exe）挂载导出根目录的 data.pak 后，所有资源读取走 pak；
// 未挂载或未命中的路径自动落盘（编辑器/开发期松散文件、存档 saves/ 不受影响）。
// 打包器 WritePak 供编辑器导出使用；读写同 TU 便于 round-trip 单测。
//
// .pak 二进制布局（小端，x86 主机约定）：
//   头部: char magic[4]="MGPK" | u32 version=1 | u32 entry_count | u64 index_offset
//   数据: 各文件原始字节（紧随头部）
//   索引(尾部): 每 entry = u16 path_len | char path[] | u64 offset | u64 size
#include <cstdint>
#include <string>
#include <vector>

namespace mg {
namespace vfs {

// 路径规范化：统一 '/' 分隔、去前导 "./"（建包与查找共用，保证 "./assets/x.png" 命中 "assets/x.png"）
std::string NormalizePath(const std::string& path);

// 挂载/卸载 pak（重复调用替换挂载；挂载后 ReadFile 优先查包）
bool MountPak(const std::string& pak_path);
void UnmountPak();
bool IsPakMounted();

// 读取文件全部字节：pak 命中 → 读包；否则落盘（Win32 宽字符 API，兼容 UTF-8 中文路径）
bool ReadFile(const std::string& path, std::vector<uint8_t>& out);

// 存在性：pak 内含 || 磁盘存在
bool Exists(const std::string& path);

// 打包：把 root_dir 下的 rel_paths（相对路径列表）写入 out_pak
bool WritePak(const std::string& root_dir, const std::vector<std::string>& rel_paths,
              const std::string& out_pak);

} // namespace vfs
} // namespace mg
