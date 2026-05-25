#define _CRT_SECURE_NO_WARNINGS
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_IE 0x0600

#include <Windows.h>
#include <tchar.h>
#include <json/json.h>
#include <fstream>
#include <ctime>
#include <string>
#include <ShlObj.h>
#include <direct.h>
#include <stdio.h>
#include <cstdlib>
#include "resource.h"
#include <objbase.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

//+++++++++++++++++++++++++
//         初始化          +
//+++++++++++++++++++++++++

std::wstring RootDir;
std::wstring ManagedSooDir;
std::wstring LnkBackupDir;

INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };

void CreateSoo(const std::wstring& sooFileName, const std::wstring& path, const std::wstring& args,
    long long currentStartTime, int preventTime);
bool CreateManaged(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner);
void BackupDesktopAllLnk(HWND hParent);
void RestoreBackupToDesktops(const std::wstring& backupFolder, HWND hParent);
void ConvertDesktopAllLnk(HWND hParent);

std::vector<int> BakRoots = { CSIDL_DESKTOP, CSIDL_COMMON_DESKTOPDIRECTORY };

//+++++++++++++++++++++++++
//        辅助函数         +
//+++++++++++++++++++++++++

std::string GetPathHash(const std::string& utf8Str) {
    std::hash<std::string> hasher;
    size_t hash = hasher(utf8Str);
    return std::to_string(hash);
}

std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    std::wstring result(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}

void MakeDir(const std::wstring& path) {
    wchar_t tempPath[MAX_PATH];
    wcscpy_s(tempPath, MAX_PATH, path.c_str());
    for (int i = 0; tempPath[i] != L'\0'; i++) {
        if (tempPath[i] == L'\\') {
            tempPath[i] = L'\0';
            _wmkdir(tempPath);
            tempPath[i] = L'\\';
        }
    }
    _wmkdir(tempPath);
}

void GetDirFromPath(const std::wstring& filePath, std::wstring& outDir) {
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        outDir = filePath.substr(0, pos + 1);
    }
    else {
        outDir = L".";
    }
}

bool CreateShortcutWithIcon(const std::wstring& lnkPath, const std::wstring& targetExe,
    const std::wstring& arguments, const std::wstring& iconSource) {
    CoInitialize(NULL);
    bool success = false;
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetExe.c_str());
        psl->SetArguments(arguments.c_str());
        psl->SetIconLocation(iconSource.c_str(), 0);
        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(lnkPath.c_str(), TRUE);
            success = SUCCEEDED(hr);
            ppf->Release();
        }
        psl->Release();
    }
    CoUninitialize();
    return success;
}

std::wstring GetFileNameWithoutExt(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    size_t slash = path.find_last_of(L"\\/");
    size_t start = (slash == std::wstring::npos) ? 0 : slash + 1;
    if (dot != std::wstring::npos && dot > start) {
        return path.substr(start, dot - start);
    }
    return path.substr(start);
}

std::wstring GetSOOPath(const std::wstring& targetExePath, const std::wstring& args) {
    std::string combined = WideToUTF8(targetExePath) + "|" + WideToUTF8(args);
    std::string hash = GetPathHash(combined);
    std::wstring path = ManagedSooDir + L"\\" + UTF8ToWide(hash) + L".soo";
    return path;
}

bool FileExists(const std::wstring& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

std::wstring GetCurrentTimestampStr() {
    std::time_t CurrentTime = std::time(nullptr);
    wchar_t buf[64];
    swprintf_s(buf, 64, L"%lld", (long long)CurrentTime);
    return std::wstring(buf);
}

std::wstring TimestampToDisplay(const std::wstring& timestamp) {
    char* wTimeStamp = (char*)timestamp.data();
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
        (int)year, (int)month, (int)day,
        (int)hour, (int)minute, (int)second);
    return std::wstring(buf);
}

bool CopyDir(const std::wstring source, const std::wstring destination) {
    if (!CreateDirectoryW(destination.c_str(), NULL)) {
        return false;
    }
    std::wstring searchPath = source;
    if (!searchPath.empty() && searchPath.back() != L'\\')
        searchPath += L'\\';
    searchPath += L'*';
    LPWIN32_FIND_DATAW data;
    HANDLE file = FindFirstFileW(LPWSTR(source.c_str()), data);
    if (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        CopyDir(source + data->cFileName, destination + data->cFileName);
    }
    FindNextFileW(file, data);
    BOOL bResult;
    CopyFileW((source + data->cFileName).data(), (destination + data->cFileName).data(), bResult);
    FindClose(file);
}

// 统一递归备份函数（所有备份共用）
void BackupLnkRecursive(const std::wstring& srcDir, const std::wstring& dstDir) {
    CreateDirectoryW(dstDir.c_str(), NULL);
    std::wstring search = srcDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring srcPath = srcDir + L"\\" + fd.cFileName;
        std::wstring dstPath = dstDir + L"\\" + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            BackupLnkRecursive(srcPath, dstPath);
        }
        else {
            const wchar_t* ext = wcsrchr(fd.cFileName, L'.');
            if (ext && _wcsicmp(ext, L".lnk") == 0) {
                CopyFileW(srcPath.c_str(), dstPath.c_str(), TRUE);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// 统一递归还原函数（强制覆盖，无嵌套）
void RestoreLnkRecursive(const std::wstring& srcDir, const std::wstring& dstDir) {
    //CreateDirectoryW(dstDir.c_str(), NULL);
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
            CopyFileW(srcPath.c_str(), dstPath.c_str(), FALSE);
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

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
    sort(timestamps.begin(), timestamps.end(), std::greater<std::wstring>());
    return timestamps;
}

bool ConvertSingleLnkToManaged(const std::wstring& lnkPath) {
    CoInitialize(NULL);
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) { CoUninitialize(); return false; }

    IPersistFile* ppf = NULL;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) { psl->Release(); CoUninitialize(); return false; }

    hr = ppf->Load(lnkPath.c_str(), STGM_READ);
    if (FAILED(hr)) { ppf->Release(); psl->Release(); CoUninitialize(); return false; }

    wchar_t target[MAX_PATH] = { 0 };
    wchar_t args[1024] = { 0 };
    psl->GetPath(target, MAX_PATH, NULL, SLGP_UNCPRIORITY);
    psl->GetArguments(args, 1024);
    ppf->Release();
    psl->Release();
    CoUninitialize();

    std::wstring targetStr = target;
    if (targetStr.length() > 4 && targetStr.substr(targetStr.length() - 4) == L".soo")
        return true;

    std::wstring folder;
    GetDirFromPath(lnkPath, folder);
    std::wstring fileName = GetFileNameWithoutExt(lnkPath);
    return CreateManaged(targetStr, args, fileName, 10, folder, true, NULL);
}

void CollectLnkFiles(const std::wstring& folder, std::vector<std::wstring>& outFiles) {
    std::wstring search = folder + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = folder + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CollectLnkFiles(fullPath, outFiles);
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

std::wstring GetSystemPath(int type) {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, type, NULL, 0, path))) return path;
    return L"";
}

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

void RunAsAdmin() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

//+++++++++++++++++++++++++
//       工作目录创建       +
//+++++++++++++++++++++++++

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

void SetRegString(HKEY root, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& data) {
    HKEY hKey;
    if (RegCreateKeyExW(root, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, valueName.empty() ? NULL : valueName.c_str(), 0, REG_SZ,
            (const BYTE*)data.c_str(), (DWORD)((data.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void Regist() {
    const wchar_t displayNameForGuide[] = L"SOO向导(双击启动向导)";
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring cmdLine = std::wstring(L"\"") + exePath + L"\" /new \"%1\"";

    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.soog", L"", L"StartOnlyOneGuide");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOneGuide", L"", displayNameForGuide);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.soog\\ShellNew", L"NullFile", L"");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOneGuide\\shell\\open\\command", L"", cmdLine);

    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.soo", L"", L"StartOnlyOne.soo");
    std::wstring openCmdLine = std::wstring(L"\"") + exePath + L"\" \"%1\"";
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOne.soo\\shell\\open\\command", L"", openCmdLine);
    std::wstring iconPath = std::wstring(L"\"") + exePath + L"\",0";
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOne.soo\\DefaultIcon", L"", iconPath);
}

void UnRegist() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.soog");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOneGuide");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.soo");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\StartOnlyOne.soo");
}

void Init() {
    InitDir();
    Regist();
}

//+++++++++++++++++++++++++
//       SOO核心逻辑       +
//+++++++++++++++++++++++++

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
    CreateSoo(file, path, args, (long long)CurrentTime, preventTime);
    if (CurrentTime - currentStartTime < preventTime) return;
    ShellExecuteW(NULL, L"open", path.c_str(), args.empty() ? NULL : args.c_str(), NULL, SW_SHOWNORMAL);
}

//+++++++++++++++++++++++++
//          引导           +
//+++++++++++++++++++++++++

static std::wstring g_defaultFolder;

bool CreateManaged(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner) {
    std::wstring sooPath = GetSOOPath(targetExe, userArgs);
    CreateSoo(sooPath, targetExe, userArgs, 0, cooldown);

    std::wstring lnkFileName = userFileName + L".lnk";
    std::wstring baseLnkPath = defaultFolder + L"\\" + lnkFileName;

    if (FileExists(baseLnkPath) && !overwrite) {
        MessageBoxW(hwndOwner, L"快捷方式已存在！", L"提示", MB_ICONWARNING);
        return false;
    }
    DeleteFileW(baseLnkPath.c_str());
    return CreateShortcutWithIcon(baseLnkPath, sooPath, L"", targetExe);
}

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

INT_PTR CALLBACK WizardProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        RECT rcDlg;
        GetWindowRect(hDlg, &rcDlg);
        RECT rcScreen;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        if (lParam) {
            g_defaultFolder = (wchar_t*)lParam;
            size_t pos = g_defaultFolder.find_last_of(L"\\/");
            if (pos != std::wstring::npos) g_defaultFolder = g_defaultFolder.substr(0, pos + 1);
        }
        CheckRadioButton(hDlg, IDC_MODE_MANAGED, IDC_MODE_FREE, IDC_MODE_MANAGED);
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, 5, FALSE);
        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BROWSE_BTN: {
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
                std::wstring defaultName = GetFileNameWithoutExt(filePath);
                SetDlgItemTextW(hDlg, IDC_NAME_EDIT, defaultName.c_str());
            }
            return TRUE;
        }
        case IDOK: {
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

void Guide(const std::wstring& soogPath) {
    std::wstring folder;
    GetDirFromPath(soogPath, folder);
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_WIZARD), NULL, WizardProc, (LPARAM)folder.c_str());
    if (result == IDOK) DeleteFileW(soogPath.c_str());
}

//+++++++++++++++++++++++++
//       桌面备份/还原      +
//+++++++++++++++++++++++++


void BackupDesktopAllLnk(HWND hParent) {
    std::wstring backupRoot = LnkBackupDir + L"\\" + GetCurrentTimestampStr();
    MakeDir(backupRoot);

    BackupLnkRecursive(GetSystemPath(CSIDL_DESKTOP), backupRoot + L"\\CurrentUser");
    BackupLnkRecursive(GetSystemPath(CSIDL_COMMON_DESKTOPDIRECTORY), backupRoot + L"\\Public");
    MessageBoxW(hParent, L"备份完成！", L"提示", MB_OK);
}

void RestoreBackupToDesktops(const std::wstring& backupFolder, HWND hParent) {
    RestoreLnkRecursive(backupFolder + L"\\CurrentUser", GetSystemPath(CSIDL_DESKTOP));
    RestoreLnkRecursive(backupFolder + L"\\Public", GetSystemPath(CSIDL_COMMON_DESKTOPDIRECTORY));
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}


void ConvertDesktopAllLnk(HWND hParent) {
    auto desktops = GetAllBakRoots();
    std::vector<std::wstring> lnks;
    for (auto& d : desktops) CollectLnkFiles(d, lnks);

    int cnt = 0;
    for (auto& lnk : lnks) {
        if (ConvertSingleLnkToManaged(lnk)) cnt++;
    }

    wchar_t msg[256];
    swprintf(msg, 256, L"转换完成：%d/%d 个快捷方式", cnt, (int)lnks.size());
    MessageBoxW(hParent, msg, L"结果", MB_OK);
}

//+++++++++++++++++++++++++
//小工具界面
//+++++++++++++++++++++++++

static std::vector<std::wstring> g_backupList;
static std::wstring g_selectedBackup;

INT_PTR CALLBACK SelectBackupProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
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
            for (auto& ts : g_backupList) {
                if (TimestampToDisplay(ts) == buf) {
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

INT_PTR CALLBACK ToolMainProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BACKUP_DESKTOP_ALL:
            BackupDesktopAllLnk(hDlg);
            return TRUE;
        case IDC_RESTORE_BACKUP: {
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"无备份！", L"提示", MB_OK); return TRUE; }
            if (DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SELECT_BACKUP), hDlg, SelectBackupProc, (LPARAM)&list) == IDOK) {
                RestoreBackupToDesktops(LnkBackupDir + L"\\" + g_selectedBackup, hDlg);
            }
            return TRUE;
        }
        case IDC_CONVERT_DESKTOP_ALL: {
            auto list = GetBackupTimestamps(LnkBackupDir);
            if (list.empty()) { MessageBoxW(hDlg, L"请先备份！", L"提示", MB_OK); return TRUE; }
            ConvertDesktopAllLnk(hDlg);
            return TRUE; 
        }
        case IDC_BACKUP_AND_CONVERT:
            BackupDesktopAllLnk(hDlg);
            ConvertDesktopAllLnk(hDlg);
            MessageBoxW(hDlg, L"备份+转换完成！", L"提示", MB_OK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

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
        if (!IsRunningAsAdmin()) {
            RunAsAdmin();
            return;
        }
        ToolMain();
    }
    else if (argc >= 3 && !wcscmp(argv[1], L"/new")) Guide(argv[2]);
    else if (argc == 2) ReadSoo(argv[1]);

    LocalFree(argv);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    //return 0;
    Init();
    InitCommonControlsEx(&icc);
    CmdLinePros();
    return 0;
}