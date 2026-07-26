#ifndef SOO_EX_H
#define SOO_EX_H
#endif // !SOO_EX_H

#include <string>
#include <ShlObj.h>
#include "GetPaths.h"
#include "SOO_Kernel.h"

// 托管模式：创建 SOO 文件，并在 defaultFolder 下生成一个指向该 SOO 的快捷方式
// 快捷方式的图标取自 targetExe
bool CreateManaged(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner, std::wstring iconSource,
    int iconIndex);

// 自由模式：仅在 defaultFolder 下创建 SOO 文件，不创建快捷方式
bool CreateFree(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner);

// 将一个快捷方式转换为托管模式（目标改为对应的 .soo 文件）
// 步骤：
//   1. 读取原快捷方式的目标程序和参数。
//   2. 如果目标已经是 .soo 则直接成功（跳过）。
//   3. 提取原快捷方式所在目录和文件名（不含扩展名）。
//   4. 调用 CreateManaged 创建新的快捷方式（覆盖原文件），冷却时间使用传入的 cooldown。
bool ConvertSingleLnkToManaged(const std::wstring& lnkPath, int cooldown);

// 转换全部：将当前用户桌面、公共桌面、任务栏中的所有 .lnk 文件转换为托管模式
// 转换时会使用传入的冷却时间，并且如果快捷方式已经是托管模式则跳过
void ConvertDesktopAllLnk(HWND hParent, int cooldown);
