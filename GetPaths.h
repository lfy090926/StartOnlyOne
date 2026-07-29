#ifndef GETPATHS_H
#define GETPATHS_H
#endif // !GETPATHS_H

#include <string>
#include <vector>
#include <Windows.h>
#include <ShlObj.h>
#include "common.h"
#include "CharConvert.h"
#include "FileOperation.h"

// 递归收集文件夹下所有 .lnk 文件的完整路径（包括子文件夹）
void CollectLnkFiles(const std::wstring& folder, std::vector<std::wstring>& outFiles);

// 根据 CSIDL 获取系统特殊文件夹路径
std::wstring GetSystemPath(int type);

// 获取所有需要备份的根路径（目前是当前用户桌面、公共桌面）
std::vector<std::wstring> GetAllBakRoots();

// 获取任务栏固定快捷方式的存储路径
// Windows 10/11 中任务栏固定项存放在：%AppData%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar
std::wstring GetTaskbarPinnedPath();

// 计算 SOO 文件的存储路径（基于 目标程序路径 + 参数 + 冷却时间 的哈希值）
// 注意：冷却时间也参与哈希，因此不同冷却时间会生成不同的 SOO 文件（隔离配置）
std::wstring GetSOOPath(const std::wstring& targetExePath, const std::wstring& args, int cooldown);

// 从完整路径中提取目录部分（包含末尾反斜杠）
void GetDirFromPath(const std::wstring& filePath, std::wstring& outDir);
