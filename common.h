#ifndef COMMON_H
#define COMMON_H
#endif // !COMMON_H

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
// 以下链接指令使程序支持 Windows 视觉样式（圆角控件、悬停效果等）
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <string>
#include <vector>

#define _WIN32_IE 0x0600          // 要求公共控件版本为 6.0 或以上（启用视觉样式）
#define COOLDOWN 180              // 全局默认冷却时间（秒），用于向导中的初始值
#define VERSION L"1.1.1"          //版本号
#define COPYRIGHT L"2026 李丰毅"   //著作权信息


extern std::wstring RootDir;          // 程序数据根目录：%APPDATA%\StartOnlyOne
extern std::wstring ManagedSooDir;    // 托管模式 SOO 文件存放目录：根目录\soo_profiles
extern std::wstring LnkBackupDir;     // 快捷方式备份目录：根目录\lnk_backup

