#define _CRT_SECURE_NO_WARNINGS
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_IE 0x0600          // 要求公共控件版本为 6.0 或以上（启用视觉样式）
#define COOLDOWN 180              // 全局默认冷却时间（秒），用于向导中的初始值

#include <Windows.h>
#include <tchar.h>
#include <json/json.h>            // 第三方 JSON 库，用于读写 SOO 配置文件
#include <fstream>
#include <ctime>
#include <string>
#include <ShlObj.h>               // Shell 对象，获取系统文件夹路径（桌面、AppData 等）
#include <direct.h>               // _wmkdir 等目录操作
#include <stdio.h>
#include <cstdlib>
#include "resource.h"             // 资源 ID 定义（对话框控件、图标等）
#include <objbase.h>              // COM 基础（CoInitialize/CoUninitialize）
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>             // 公共控件（列表视图等）
#include <functional>
#include <vector>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
// 以下链接指令使程序支持 Windows 视觉样式（圆角控件、悬停效果等）
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//+++++++++++++++++++++++++
//         初始化          +
//+++++++++++++++++++++++++

std::wstring RootDir;          // 程序数据根目录：%APPDATA%\StartOnlyOne
std::wstring ManagedSooDir;    // 托管模式 SOO 文件存放目录：根目录\soo_profiles
std::wstring LnkBackupDir;     // 快捷方式备份目录：根目录\lnk_backup

// 公共控件初始化结构体，用于 InitCommonControlsEx
INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };

// 函数前向声明（因为存在相互调用）
void CreateSoo(const std::wstring& sooFileName, const std::wstring& path, const std::wstring& args,
    long long currentStartTime, int preventTime);
bool CreateManaged(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner);
void BackupDesktopAllLnk(HWND hParent);
void RestoreBackupToDesktops(const std::wstring& backupFolder, HWND hParent);
void ConvertDesktopAllLnk(HWND hParent, int cooldown);

// 需要备份的根目录列表（CSIDL 常量）：当前用户桌面、公共桌面
// 注意：程序启动时会读取此列表，未来可扩展其他位置（如任务栏、启动文件夹）
std::vector<int> BakRoots = { CSIDL_DESKTOP, CSIDL_COMMON_DESKTOPDIRECTORY };

//+++++++++++++++++++++++++
//        辅助函数         +
//+++++++++++++++++++++++++

// 获取字符串的哈希值（用于生成 SOO 文件名）
// 参数: utf8Str - UTF-8 编码的字符串
// 返回: 十进制哈希字符串
std::string GetPathHash(const std::string& utf8Str) {
    std::hash<std::string> hasher;
    size_t hash = hasher(utf8Str);
    return std::to_string(hash);
}

// UTF-8 字符串转宽字符串（Windows API 大多使用 UTF-16 宽字符）
std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    // 计算所需缓冲区大小（包括结尾的 L'\0'）
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    std::wstring result(len - 1, 0);           // 减去结尾的 L'\0'
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

// 宽字符串转 UTF-8（用于 JSON 序列化，便于保存非 ASCII 字符）
std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}

// 递归创建目录（支持多级路径，如 "a\b\c"）
// 原理：逐个创建每级子目录，若已存在则跳过
void MakeDir(const std::wstring& path) {
    wchar_t tempPath[MAX_PATH];
    wcscpy_s(tempPath, MAX_PATH, path.c_str());
    for (int i = 0; tempPath[i] != L'\0'; i++) {
        if (tempPath[i] == L'\\') {
            tempPath[i] = L'\0';
            _wmkdir(tempPath);          // 创建当前已截断的部分
            tempPath[i] = L'\\';
        }
    }
    _wmkdir(tempPath);                  // 创建最后一级
}

// 从完整路径中提取目录部分（包含末尾反斜杠）
void GetDirFromPath(const std::wstring& filePath, std::wstring& outDir) {
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        outDir = filePath.substr(0, pos + 1);
    }
    else {
        outDir = L".";                  // 当前目录
    }
}

// 创建指向 targetExe 的快捷方式，并设置图标为 iconSource 的第一个图标
// 参数:
//   lnkPath     - 要创建的 .lnk 文件完整路径
//   targetExe   - 快捷方式的目标程序（在本程序中通常是 SOO 文件路径）
//   arguments   - 命令行参数（为空则无参数）
//   iconSource  - 图标的来源文件（通常是目标 exe，也可能是 dll 等）
bool CreateShortcutWithIcon(const std::wstring& lnkPath, const std::wstring& targetExe,
    const std::wstring& arguments, const std::wstring& iconSource) {
    CoInitialize(NULL);          // 初始化 COM（ShellLink 需要）
    bool success = false;
    IShellLinkW* psl = NULL;
    // 创建 ShellLink 对象
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetExe.c_str());
        psl->SetArguments(arguments.c_str());
        psl->SetIconLocation(iconSource.c_str(), 0);   // 0 表示文件中的第一个图标
        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(lnkPath.c_str(), TRUE);     // TRUE 表示文件名已包含路径
            success = SUCCEEDED(hr);
            ppf->Release();
        }
        psl->Release();
    }
    CoUninitialize();
    return success;
}

// 从完整路径提取文件名（不含扩展名）
// 例如: "C:\Program Files\QQ\QQ.exe" -> "QQ"
std::wstring GetFileNameWithoutExt(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    size_t slash = path.find_last_of(L"\\/");
    size_t start = (slash == std::wstring::npos) ? 0 : slash + 1;
    if (dot != std::wstring::npos && dot > start) {
        return path.substr(start, dot - start);
    }
    return path.substr(start);
}

// 计算 SOO 文件的存储路径（基于 目标程序路径 + 参数 + 冷却时间 的哈希值）
// 注意：冷却时间也参与哈希，因此不同冷却时间会生成不同的 SOO 文件（隔离配置）
std::wstring GetSOOPath(const std::wstring& targetExePath, const std::wstring& args, int cooldown) {
    wchar_t wcCooldown[64];
    swprintf_s(wcCooldown, 64, L"%d", cooldown);                      // 整型转宽字符串
    std::string combined = WideToUTF8(targetExePath) + "|" + WideToUTF8(args) + "|" + WideToUTF8(wcCooldown);
    std::string hash = GetPathHash(combined);
    std::wstring path = ManagedSooDir + L"\\" + UTF8ToWide(hash) + L".soo";
    return path;
}

// 判断文件或目录是否存在（使用 std::ifstream，简单但可靠）
bool FileExists(const std::wstring& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

// 获取当前时间的 Unix 时间戳（秒数），作为备份文件夹的名称
std::wstring GetCurrentTimestampStr() {
    std::time_t CurrentTime = std::time(nullptr);
    wchar_t buf[64];
    swprintf_s(buf, 64, L"%lld", (long long)CurrentTime);
    return std::wstring(buf);
}

// 将 Unix 时间戳字符串转换为本地日期时间字符串（用于界面显示）
std::wstring TimestampToDisplay(const std::wstring& timestamp) {
    char* wTimeStamp = (char*)timestamp.data();   // 注意：这里转换方式较危险，实际应使用 std::stoll
    std::time_t CurrentTime = std::stoll(timestamp);
    struct tm LocalTime;
    localtime_s(&LocalTime, &CurrentTime);
    int year = LocalTime.tm_year + 1900,
        month = LocalTime.tm_mon + 1,
        day = LocalTime.tm_mday,
        hour = LocalTime.tm_hour,
        minute = LocalTime.tm_min,
        second = LocalTime.tm_sec;
    wchar_t buf[64];
    swprintf(buf, 64, L"%04d年%02d月%02d日 %02d:%02d:%02d",
        year, month, day, hour, minute, second);
    return std::wstring(buf);
}

// 递归备份：将源目录 srcDir 下的所有 .lnk 文件复制到 dstDir 中，保持子目录结构
// 注意：仅复制 .lnk，其他文件忽略；目标目录会自动创建
void BackupLnkRecursive(const std::wstring& srcDir, const std::wstring& dstDir) {
    CreateDirectoryW(dstDir.c_str(), NULL);                // 创建目标目录（若已存在则无影响）
    std::wstring search = srcDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring srcPath = srcDir + L"\\" + fd.cFileName;
        std::wstring dstPath = dstDir + L"\\" + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            BackupLnkRecursive(srcPath, dstPath);          // 递归子目录
        }
        else {
            const wchar_t* ext = wcsrchr(fd.cFileName, L'.');
            if (ext && _wcsicmp(ext, L".lnk") == 0) {      // 仅处理 .lnk 文件
                CopyFileW(srcPath.c_str(), dstPath.c_str(), TRUE); // TRUE 表示若目标存在则失败，此处利用此特性避免覆盖？实际上备份时建议覆盖，但用 TRUE 更安全。
                // 注意：若备份时目标已存在，CopyFile 会失败，但通常备份目标不存在，此处无大碍。
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// 递归还原：将 srcDir 中的所有内容强制复制到 dstDir 中（覆盖同名文件）
// 与备份不同，此处不限制扩展名，因为备份目录中只有 .lnk 文件（但为了通用，所有文件都复制）
void RestoreLnkRecursive(const std::wstring& srcDir, const std::wstring& dstDir) {
    std::wstring search = srcDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring srcPath = srcDir + L"\\" + fd.cFileName;
        std::wstring dstPath = dstDir + L"\\" + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RestoreLnkRecursive(srcPath, dstPath);
        }
        else {
            CopyFileW(srcPath.c_str(), dstPath.c_str(), FALSE);  // FALSE 表示覆盖已存在的文件
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// 获取备份根目录下的所有时间戳子目录（文件夹名就是时间戳），并按时间倒序排序（最新的在前）
std::vector<std::wstring> GetBackupTimestamps(const std::wstring& backupDir) {
    std::vector<std::wstring> timestamps;
    std::wstring searchPath = backupDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return timestamps;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                timestamps.push_back(fd.cFileName);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    std::sort(timestamps.begin(), timestamps.end(), std::greater<std::wstring>());
    return timestamps;
}

// 将一个快捷方式转换为托管模式（目标改为对应的 .soo 文件）
// 步骤：
//   1. 读取原快捷方式的目标程序和参数。
//   2. 如果目标已经是 .soo 则直接成功（跳过）。
//   3. 提取原快捷方式所在目录和文件名（不含扩展名）。
//   4. 调用 CreateManaged 创建新的快捷方式（覆盖原文件），冷却时间使用传入的 cooldown。
bool ConvertSingleLnkToManaged(const std::wstring& lnkPath, int cooldown) {
    CoInitialize(NULL);
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) { CoUninitialize(); return false; }

    IPersistFile* ppf = NULL;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) { psl->Release(); CoUninitialize(); return false; }

    hr = ppf->Load(lnkPath.c_str(), STGM_READ);   // 以只读方式加载现有快捷方式
    if (FAILED(hr)) { ppf->Release(); psl->Release(); CoUninitialize(); return false; }

    wchar_t target[MAX_PATH] = { 0 };
    wchar_t args[1024] = { 0 };
    psl->GetPath(target, MAX_PATH, NULL, SLGP_UNCPRIORITY);
    psl->GetArguments(args, 1024);
    ppf->Release();
    psl->Release();
    CoUninitialize();

    std::wstring targetStr = target;
    // 如果目标已经是 .soo 文件，则无需转换
    if (targetStr.length() > 4 && targetStr.substr(targetStr.length() - 4) == L".soo")
        return true;

    // 提取原快捷方式的目录和文件名（不含扩展名）
    std::wstring folder;
    GetDirFromPath(lnkPath, folder);
    std::wstring fileName = GetFileNameWithoutExt(lnkPath);
    // 调用 CreateManaged 创建新的快捷方式（overwrite=true 会删除原文件并创建新的）
    return CreateManaged(targetStr, args, fileName, cooldown, folder, true, NULL);
}

// 递归收集文件夹下所有 .lnk 文件的完整路径（包括子文件夹）
void CollectLnkFiles(const std::wstring& folder, std::vector<std::wstring>& outFiles) {
    std::wstring search = folder + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = folder + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CollectLnkFiles(fullPath, outFiles);       // 递归子文件夹
        }
        else {
            const wchar_t* ext = wcsrchr(fd.cFileName, L'.');
            if (ext && _wcsicmp(ext, L".lnk") == 0) {
                outFiles.push_back(fullPath);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// 根据 CSIDL 获取系统特殊文件夹路径
std::wstring GetSystemPath(int type) {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, type, NULL, 0, path))) return path;
    return L"";
}

// 获取所有需要备份的根路径（目前是当前用户桌面、公共桌面）
std::vector<std::wstring> GetAllBakRoots() {
    std::vector<std::wstring> paths;
    for (int type : BakRoots) {
        std::wstring path = GetSystemPath(type);
        if (!path.empty()) {
            paths.push_back(path);
        }
    }
    return paths;
}

// 获取任务栏固定快捷方式的存储路径
// Windows 10/11 中任务栏固定项存放在：%AppData%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar
std::wstring GetTaskbarPinnedPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::wstring(path) + L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
    }
    return L"";
}

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

//+++++++++++++++++++++++++
//      工作目录创建       +
//+++++++++++++++++++++++++

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

//+++++++++++++++++++++++++
//       注册表的注册       +
//+++++++++++++++++++++++++

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

//+++++++++++++++++++++++++
//       SOO核心逻辑       +
//+++++++++++++++++++++++++

// 检查 SOO 文件是否有效：存在、非空、JSON 格式正确、包含必要字段
bool IsSooFileValid(const std::wstring& filePath) {
    std::ifstream ifs(WideToUTF8(filePath));
    if (!ifs.is_open()) return false;
    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    if (fileSize == 0) { ifs.close(); return false; }
    Json::Reader reader;
    Json::Value root;
    bool parseOk = reader.parse(ifs, root);
    ifs.close();
    if (!parseOk) return false;
    if (!root.isObject()) return false;
    if (!root.isMember("path") || !root.isMember("currentStartTime") || !root.isMember("preventTime") || !root.isMember("args")) return false;
    if (!root["path"].isString() || !root["currentStartTime"].isInt() || !root["preventTime"].isInt() || !root["args"].isString()) return false;
    return true;
}

// 创建 SOO 文件，内容为 JSON 格式
void CreateSoo(const std::wstring& sooFileName, const std::wstring& path, const std::wstring& args,
    long long currentStartTime, int preventTime) {
    Json::Value soo;
    soo["path"] = WideToUTF8(path);
    soo["args"] = WideToUTF8(args);
    soo["currentStartTime"] = (Json::Int64)currentStartTime;
    soo["preventTime"] = preventTime;
    Json::StyledWriter writer;
    std::string file = writer.write(soo);
    std::ofstream ofs(WideToUTF8(sooFileName));
    ofs << file;
    ofs.close();
}

// 读取 SOO 文件，检查冷却时间并启动目标程序
void ReadSoo(const std::wstring& file) {
    if (!IsSooFileValid(file)) {
        MessageBoxW(NULL, L"文件可能损坏或无读取权限!", L"警告", MB_ICONWARNING | MB_OK);
        return;
    }
    std::ifstream ifs(WideToUTF8(file));
    Json::Reader rd;
    Json::Value root;
    rd.parse(ifs, root);
    ifs.close();
    std::wstring path = UTF8ToWide(root["path"].asString());
    long long currentStartTime = root["currentStartTime"].asInt64();
    int preventTime = root["preventTime"].asInt();
    std::wstring args = UTF8ToWide(root["args"].asString());
    std::time_t CurrentTime = std::time(nullptr);
    // 重要：先更新 SOO 文件中的 currentStartTime 为当前时间（即使不启动也要更新，防止短时间频繁请求）
    CreateSoo(file, path, args, (long long)CurrentTime, preventTime);
    // 冷却判断：如果距离上次启动时间不足 preventTime 秒，则不启动
    if (CurrentTime - currentStartTime < preventTime) return;
    ShellExecuteW(NULL, L"open", path.c_str(), args.empty() ? NULL : args.c_str(), NULL, SW_SHOWNORMAL);
}

//+++++++++++++++++++++++++
//          引导           +
//+++++++++++++++++++++++++

static std::wstring g_defaultFolder;   // 用于在向导对话框间传递用户选择的文件夹路径

// 托管模式：创建 SOO 文件，并在 defaultFolder 下生成一个指向该 SOO 的快捷方式
// 快捷方式的图标取自 targetExe
bool CreateManaged(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner) {
    std::wstring sooPath = GetSOOPath(targetExe, userArgs, cooldown);
    CreateSoo(sooPath, targetExe, userArgs, 0, cooldown);

    std::wstring lnkFileName = userFileName + L".lnk";
    std::wstring baseLnkPath = defaultFolder + L"\\" + lnkFileName;

    if (FileExists(baseLnkPath) && !overwrite) {
        MessageBoxW(hwndOwner, L"快捷方式已存在！", L"提示", MB_ICONWARNING);
        return false;
    }
    // 删除已有快捷方式（如果存在），确保覆盖或重新创建
    DeleteFileW(baseLnkPath.c_str());
    return CreateShortcutWithIcon(baseLnkPath, sooPath, L"", targetExe);
}

// 自由模式：仅在 defaultFolder 下创建 SOO 文件，不创建快捷方式
bool CreateFree(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner) {
    std::wstring sooFileName = userFileName + L".soo";
    std::wstring finalSooPath = defaultFolder + L"\\" + sooFileName;

    if (FileExists(finalSooPath) && !overwrite) {
        MessageBoxW(hwndOwner, L"SOO文件已存在！", L"提示", MB_ICONWARNING);
        return false;
    }
    DeleteFileW(finalSooPath.c_str());
    CreateSoo(finalSooPath, targetExe, userArgs, 0, cooldown);
    return true;
}

// 新建向导的对话框过程
INT_PTR CALLBACK WizardProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        // 窗口居中于屏幕工作区
        RECT rcDlg;
        GetWindowRect(hDlg, &rcDlg);
        RECT rcScreen;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置对话框图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // 保存用户右键新建时所在的文件夹（从 lParam 传入）
        if (lParam) {
            g_defaultFolder = (wchar_t*)lParam;
            size_t pos = g_defaultFolder.find_last_of(L"\\/");
            if (pos != std::wstring::npos) g_defaultFolder = g_defaultFolder.substr(0, pos + 1);
        }
        // 默认选中“托管模式”
        CheckRadioButton(hDlg, IDC_MODE_MANAGED, IDC_MODE_FREE, IDC_MODE_MANAGED);
        // 设置冷却时间编辑框的默认值
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, COOLDOWN, FALSE);
        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BROWSE_BTN: {   // 浏览按钮：选择目标 exe
            OPENFILENAMEW ofn = { 0 };
            wchar_t filePath[MAX_PATH] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = L"可执行文件\0*.exe\0所有文件\0*.*\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) {
                SetDlgItemTextW(hDlg, IDC_TARGET_EDIT, filePath);
                // 自动将文件名（不含扩展名）填入“目标文件名”编辑框
                std::wstring defaultName = GetFileNameWithoutExt(filePath);
                SetDlgItemTextW(hDlg, IDC_NAME_EDIT, defaultName.c_str());
            }
            return TRUE;
        }
        case IDOK: {
            // 获取用户输入
            wchar_t targetExe[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, IDC_TARGET_EDIT, targetExe, MAX_PATH);
            if (!targetExe[0]) {
                MessageBoxW(hDlg, L"请选择程序！", L"提示", MB_ICONWARNING);
                return TRUE;
            }

            wchar_t fileName[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, IDC_NAME_EDIT, fileName, MAX_PATH);
            if (!fileName[0]) wcscpy_s(fileName, GetFileNameWithoutExt(targetExe).c_str());

            wchar_t args[1024] = { 0 };
            GetDlgItemTextW(hDlg, IDC_ARGS_EDIT, args, 1024);
            BOOL overwrite = IsDlgButtonChecked(hDlg, IDC_OVERWRITE_CHECK) == BST_CHECKED;
            BOOL isManaged = IsDlgButtonChecked(hDlg, IDC_MODE_MANAGED) == BST_CHECKED;
            int cooldown = max(0, GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE));

            bool success = isManaged ? CreateManaged(targetExe, args, fileName, cooldown, g_defaultFolder, overwrite, hDlg)
                : CreateFree(targetExe, args, fileName, cooldown, g_defaultFolder, overwrite, hDlg);
            if (success) {
                MessageBoxW(hDlg, L"创建成功！", L"完成", MB_OK);
                EndDialog(hDlg, IDOK);
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// 向导入口：由 .soog 文件双击触发，显示向导，完成后删除临时 .soog 文件
void Guide(const std::wstring& soogPath) {
    std::wstring folder;
    GetDirFromPath(soogPath, folder);
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_WIZARD), NULL, WizardProc, (LPARAM)folder.c_str());
    if (result == IDOK) DeleteFileW(soogPath.c_str());
}

//+++++++++++++++++++++++++
//       桌面备份/还原      +
//+++++++++++++++++++++++++

// 备份全部：当前用户桌面、公共桌面、任务栏固定项，分别存入备份目录下的 CurrentUser、Public、TaskBar 子目录
void BackupDesktopAllLnk(HWND hParent) {
    std::wstring backupRoot = LnkBackupDir + L"\\" + GetCurrentTimestampStr();
    MakeDir(backupRoot);   // 创建以时间戳命名的文件夹

    BackupLnkRecursive(GetSystemPath(CSIDL_DESKTOP), backupRoot + L"\\CurrentUser");
    BackupLnkRecursive(GetSystemPath(CSIDL_COMMON_DESKTOPDIRECTORY), backupRoot + L"\\Public");
    std::wstring taskbarPath = GetTaskbarPinnedPath();
    if (!taskbarPath.empty() && GetFileAttributesW(taskbarPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        BackupLnkRecursive(taskbarPath, backupRoot + L"\\TaskBar");
    }
    MessageBoxW(hParent, L"备份完成！", L"提示", MB_OK);
}

// 还原全部：从选定的备份文件夹中恢复三个子目录到原始位置
void RestoreBackupToDesktops(const std::wstring& backupFolder, HWND hParent) {
    // 恢复当前用户桌面
    RestoreLnkRecursive(backupFolder + L"\\CurrentUser", GetSystemPath(CSIDL_DESKTOP));
    // 恢复公共桌面
    RestoreLnkRecursive(backupFolder + L"\\Public", GetSystemPath(CSIDL_COMMON_DESKTOPDIRECTORY));
    // 恢复任务栏
    std::wstring taskbarBackup = backupFolder + L"\\TaskBar";
    if (GetFileAttributesW(taskbarBackup.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring taskbarPath = GetTaskbarPinnedPath();
        if (!taskbarPath.empty()) {
            RestoreLnkRecursive(taskbarBackup, taskbarPath);
        }
    }
    // 通知系统图标关联已改变，刷新图标缓存（可选）
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    MessageBoxW(hParent, L"还原完成！", L"提示", MB_OK);
}

// 转换全部：将当前用户桌面、公共桌面、任务栏中的所有 .lnk 文件转换为托管模式
// 转换时会使用传入的冷却时间，并且如果快捷方式已经是托管模式则跳过
void ConvertDesktopAllLnk(HWND hParent, int cooldown) {
    auto desktops = GetAllBakRoots();          // 当前用户桌面 + 公共桌面
    std::vector<std::wstring> lnks;
    for (auto& d : desktops) CollectLnkFiles(d, lnks);

    std::wstring taskbarPath = GetTaskbarPinnedPath();
    if (!taskbarPath.empty()) {
        CollectLnkFiles(taskbarPath, lnks);
    }

    int cnt = 0;
    for (auto& lnk : lnks) {
        if (ConvertSingleLnkToManaged(lnk, cooldown)) cnt++;
    }

    wchar_t msg[256];
    swprintf(msg, 256, L"转换完成：%d/%d 个快捷方式", cnt, (int)lnks.size());
    MessageBoxW(hParent, msg, L"结果", MB_OK);
}

//+++++++++++++++++++++++++
//小工具界面
//+++++++++++++++++++++++++

static std::vector<std::wstring> g_backupList;   // 备份时间戳列表（供选择对话框使用）
static std::wstring g_selectedBackup;            // 用户选中的时间戳

// 备份选择对话框过程
INT_PTR CALLBACK SelectBackupProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 居中窗口
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // 接收备份列表并显示在 ListView 中
        g_backupList = *(std::vector<std::wstring>*)lParam;
        HWND hList = GetDlgItem(hDlg, IDC_BACKUP_LIST);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);
        LVCOLUMNW col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<LPWSTR>(L"备份时间");
        col.cx = 250;
        ListView_InsertColumn(hList, 0, &col);

        for (int i = 0; i < g_backupList.size(); i++) {
            std::wstring display = TimestampToDisplay(g_backupList[i]);
            LVITEMW item = { 0 };
            item.mask = LVIF_TEXT;
            item.iItem = i;
            item.pszText = (wchar_t*)display.c_str();
            ListView_InsertItem(hList, &item);
        }
        return TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_BTN_RESTORE_SELECTED) {
            HWND hList = GetDlgItem(hDlg, IDC_BACKUP_LIST);
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel == -1) { MessageBoxW(hDlg, L"请选择备份！", L"提示", MB_OK); return TRUE; }
            wchar_t buf[256];
            ListView_GetItemText(hList, sel, 0, buf, 256);
            std::wstring display = buf;
            // 根据显示文本查找对应的时间戳
            for (auto& ts : g_backupList) {
                if (TimestampToDisplay(ts) == display) {
                    g_selectedBackup = ts;
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
            }
        }
        else if (LOWORD(wParam) == IDCANCEL) EndDialog(hDlg, IDCANCEL);
        break;
    }
    }
    return FALSE;
}

// 转换桌面快捷方式引导对话框（允许用户输入冷却时间）
INT_PTR CALLBACK ConvertGuideProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
    case WM_INITDIALOG: {
        // 居中
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, COOLDOWN, FALSE);
        // 设置图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        case IDOK: {
            int cooldown = max(0, GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE));
            ConvertDesktopAllLnk(hDlg, cooldown);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        }
        return FALSE;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return FALSE;
    }
    return FALSE;
}

// 备份并转换桌面快捷方式引导对话框
INT_PTR CALLBACK BackupConvertGuideProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
    case WM_INITDIALOG: {
        // 居中
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, COOLDOWN, FALSE);
        // 设置图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        case IDOK: {
            int cooldown = max(0, GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE));
            BackupDesktopAllLnk(hDlg);
            ConvertDesktopAllLnk(hDlg, cooldown);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        }
        return FALSE;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return FALSE;
    }
    return FALSE;
}

// 工具主界面过程
INT_PTR CALLBACK ToolMainProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 居中窗口
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // 设置窗口图标
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        //设置按钮文字(默认冷却时间)
        wchar_t wCOOLDOWN[64];
        swprintf_s(wCOOLDOWN, 64, L"%d", COOLDOWN);
        wchar_t text_IDC_CONVERT_DESKTOP_ALL[64];
        swprintf_s(text_IDC_CONVERT_DESKTOP_ALL, 64, L"转换桌面快捷方式 (默认设置：冷却%ls秒)", wCOOLDOWN);
        wchar_t text_IDC_BACKUP_AND_CONVERT[64];
        swprintf_s(text_IDC_BACKUP_AND_CONVERT, 64, L"一键备份并转换 (默认设置：冷却%ls秒)", wCOOLDOWN);
        SetDlgItemTextW(hDlg, IDC_CONVERT_DESKTOP_ALL, text_IDC_CONVERT_DESKTOP_ALL);
        SetDlgItemTextW(hDlg, IDC_BACKUP_AND_CONVERT, text_IDC_BACKUP_AND_CONVERT);

        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BACKUP_DESKTOP_ALL:      // 备份按钮
            BackupDesktopAllLnk(hDlg);
            return TRUE;
        case IDC_RESTORE_BACKUP: {        // 还原按钮
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"无备份！", L"提示", MB_OK); return TRUE; }
            if (DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SELECT_BACKUP), hDlg, SelectBackupProc, (LPARAM)&list) == IDOK) {
                RestoreBackupToDesktops(LnkBackupDir + L"\\" + g_selectedBackup, hDlg);
            }
            return TRUE;
        }
        case IDC_CONVERT_DESKTOP_ALL:    // 直接转换（使用默认冷却时间）
        {
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"请先备份！", L"提示", MB_OK); return TRUE; }
            ConvertDesktopAllLnk(hDlg, COOLDOWN);
            return TRUE;
        }
        case IDC_BACKUP_AND_CONVERT:     // 一键备份并转换（默认冷却时间）
        {
            BackupDesktopAllLnk(hDlg);
            ConvertDesktopAllLnk(hDlg, COOLDOWN);
            MessageBoxW(hDlg, L"备份+转换完成！", L"提示", MB_OK);
            return TRUE; 
        }
        case IDC_CONVERT_GUIDE:          // 转换向导（可自定义冷却）
        {
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"请先备份！", L"提示", MB_OK); return TRUE; }
            DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_CONVERT_GUIDE), hDlg, ConvertGuideProc, 0);
            return TRUE; 
        }
        case IDC_BACKUP_CONVERT_GUIDE:   // 备份并转换向导
        {
            DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_CONVERT_GUIDE), hDlg, BackupConvertGuideProc, 0);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// 工具主界面入口
void ToolMain() {
    DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_TOOLMAIN), NULL, ToolMainProc);
}

//+++++++++++++++++++++++++
//       处理命令行         +
//+++++++++++++++++++++++++

void CmdLinePros() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    if (argc == 1) {
        // 无参数：检查管理员权限，若无则提权；然后打开主工具界面
        if (!IsRunningAsAdmin()) {
            RunAsAdmin();
            return;
        }
        ToolMain();
    }
    else if (argc >= 3 && !wcscmp(argv[1], L"/new")) Guide(argv[2]);   // 新建向导
    else if (argc == 2) ReadSoo(argv[1]);                               // 打开 SOO 文件

    LocalFree(argv);
}

// 程序入口点
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    Init();                             // 创建目录、注册表关联
    InitCommonControlsEx(&icc);         // 初始化公共控件（使 ListView 等可用）
    CmdLinePros();
    return 0;
}