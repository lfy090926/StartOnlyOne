#ifndef FILEOPERATION_H
#define FILEOPERATION_H
#endif // !FILEOPERATION_H


#include <string>
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <vector>
#include "CharConvert.h"
#include "common.h"


// 获取字符串的哈希值（用于生成 SOO 文件名）
// 参数: utf8Str - UTF-8 编码的字符串
// 返回: 十进制哈希字符串
std::string GetPathHash(const std::string& utf8Str);

// 递归创建目录（支持多级路径，如 "a\b\c"）
// 原理：逐个创建每级子目录，若已存在则跳过
void MakeDir(const std::wstring& path);

// 创建指向 targetExe 的快捷方式，并设置图标为 iconSource 的第一个图标
// 参数:
//   lnkPath     - 要创建的 .lnk 文件完整路径
//   targetExe   - 快捷方式的目标程序（在本程序中通常是 SOO 文件路径）
//   arguments   - 命令行参数（为空则无参数）
//   iconSource  - 图标的来源文件（通常是目标 exe，也可能是 dll 等）
bool CreateShortcutWithIcon(const std::wstring& lnkPath, const std::wstring& targetExe,
    const std::wstring& arguments, const std::wstring& iconSource, int iconIndex);

// 从完整路径提取文件名（不含扩展名）
// 例如: "C:\Program Files\QQ\QQ.exe" -> "QQ"
std::wstring GetFileNameWithoutExt(const std::wstring& path);

// 判断文件或目录是否存在（使用 std::ifstream，简单但可靠）
bool FileExists(const std::wstring& path);

// 递归备份：将源目录 srcDir 下的所有 .lnk 文件复制到 dstDir 中，保持子目录结构
// 注意：仅复制 .lnk，其他文件忽略；目标目录会自动创建
void BackupLnkRecursive(const std::wstring& srcDir, const std::wstring& dstDir);

// 递归还原：将 srcDir 中的所有内容强制复制到 dstDir 中（覆盖同名文件）
// 与备份不同，此处不限制扩展名，因为备份目录中只有 .lnk 文件（但为了通用，所有文件都复制）
void RestoreLnkRecursive(const std::wstring& srcDir, const std::wstring& dstDir);

// 备份全部：当前用户桌面、公共桌面、任务栏固定项，分别存入备份目录下的 CurrentUser、Public、TaskBar 子目录
void BackupDesktopAllLnk(HWND hParent);

// 还原全部：从选定的备份文件夹中恢复三个子目录到原始位置
void RestoreBackupToDesktops(const std::wstring& backupFolder, HWND hParent);
