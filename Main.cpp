#define _CRT_SECURE_NO_WARNINGS
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "Init.h"
#include "UI.h"

//+++++++++++++++++++++++++
//         初始化          +
//+++++++++++++++++++++++++

std::wstring RootDir;          // 程序数据根目录：%APPDATA%\StartOnlyOne
std::wstring ManagedSooDir;    // 托管模式 SOO 文件存放目录：根目录\soo_profiles
std::wstring LnkBackupDir;     // 快捷方式备份目录：根目录\lnk_backup

// 公共控件初始化结构体，用于 InitCommonControlsEx
INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };

// 程序入口点
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    Init();                             // 创建目录、注册表关联
    InitCommonControlsEx(&icc);         // 初始化公共控件（使 ListView 等可用）
    //处理命令行
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return -1;

    if (argc == 1) {
        // 无参数：检查管理员权限，若无则提权；然后打开主工具界面
        if (!IsRunningAsAdmin()) {
            RunAsAdmin();
            return -1;
        }
        ToolMain();
    }
    else if (argc >= 3 && !wcscmp(argv[1], L"/new")) Guide(argv[2]);   // 新建向导
    else if (argc == 2) ReadSoo(argv[1]);                               // 打开 SOO 文件

    LocalFree(argv);

    return 0;
}