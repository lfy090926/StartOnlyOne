#include <Windows.h>
#include <ShlObj.h>
#include "common.h"
#include "FileOperation.h"

// 检查当前进程是否以管理员身份运行
BOOL IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

// 以管理员权限重新启动当前程序（会弹出 UAC 对话框）
void RunAsAdmin() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";        // 请求以管理员身份运行
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

// 初始化程序所需的三个目录（根目录、soo_profiles、lnk_backup）
void InitDir() {
    wchar_t AppData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, AppData);
    RootDir = std::wstring(AppData) + L"\\StartOnlyOne";
    ManagedSooDir = RootDir + L"\\soo_profiles";
    LnkBackupDir = RootDir + L"\\lnk_backup";
    MakeDir(RootDir);
    MakeDir(ManagedSooDir);
    MakeDir(LnkBackupDir);
}
// 辅助函数：向注册表写入一个字符串值（如果子键不存在则创建）
void SetRegString(HKEY root, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& data) {
    HKEY hKey;
    if (RegCreateKeyExW(root, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, valueName.empty() ? NULL : valueName.c_str(), 0, REG_SZ,
            (const BYTE*)data.c_str(), (DWORD)((data.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

// 注册文件关联和右键新建菜单
// 1. 注册 .soog 扩展名，用于右键“新建”菜单触发向导（ShellNew + NullFile）
// 2. 注册 .soo 扩展名，使双击 .soo 文件时用本程序打开
// 3. 为 .soo 设置默认图标（本程序自带的图标）
void Regist() {
    const wchar_t displayNameForGuide[] = L"SOO向导(双击启动向导)";
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    // 命令行：本程序 /new "%1" ，当用户双击 .soog 文件时，%1 会被替换为文件路径
    std::wstring cmdLine = std::wstring(L"\"") + exePath + L"\" /new \"%1\"";

    // .soog 关联
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.soog", L"", L"StartOnlyOneGuide");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOneGuide", L"", displayNameForGuide);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.soog\\ShellNew", L"NullFile", L"");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOneGuide\\shell\\open\\command", L"", cmdLine);

    // .soo 关联
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.soo", L"", L"StartOnlyOne.soo");
    std::wstring openCmdLine = std::wstring(L"\"") + exePath + L"\" \"%1\"";
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOne.soo\\shell\\open\\command", L"", openCmdLine);
    std::wstring iconPath = std::wstring(L"\"") + exePath + L"\",0";
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOne.soo\\DefaultIcon", L"", iconPath);
}

// 卸载程序时清除注册表项（可选项）
void UnRegist() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.soog");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOneGuide");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.soo");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOne.soo");
}

// 初始化入口：创建目录 + 注册表
void Init() {
    InitDir();
    Regist();
}
