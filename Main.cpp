#define _CRT_SECURE_NO_WARNINGS
#define UNICODE
#define _UNICODE

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

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

//+++++++++++++++++++++++++
//         初始化          +
//+++++++++++++++++++++++++

std::wstring RootDir;          // %APPDATA%\StartOnlyOne
std::wstring ManagedSooDir;    // %APPDATA%\StartOnlyOne\soo_profiles
std::wstring LnkBackupDir;     // %APPDATA%\StartOnlyOne\lnk_backup

//+++++++++++++++++++++++++
//        辅助函数         +
//+++++++++++++++++++++++++

// 获取哈希值 (基于 UTF-8 字符串)
std::string GetPathHash(const std::string& utf8Str) {
    std::hash<std::string> hasher;
    size_t hash = hasher(utf8Str);
    return std::to_string(hash);
}

// UTF-8 转 std::wstring
std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    std::wstring result(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

// std::wstring 转 UTF-8
std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}

// 创建文件夹（支持宽字符路径）
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

// 获取文件所在目录
void GetDirFromPath(const std::wstring& filePath, std::wstring& outDir) {
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        outDir = filePath.substr(0, pos + 1);
    }
    else {
        outDir = L".";
    }
}

// 创建快捷方式并指定图标来源（宽字符版本）
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

// 从完整路径提取文件名（不含扩展名）
std::wstring GetFileNameWithoutExt(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    size_t slash = path.find_last_of(L"\\/");
    size_t start = (slash == std::wstring::npos) ? 0 : slash + 1;
    if (dot != std::wstring::npos && dot > start) {
        return path.substr(start, dot - start);
    }
    return path.substr(start);
}

// SOO 文件路径计算（包含参数）
std::wstring GetSOOPath(const std::wstring& targetExePath, const std::wstring& args) {
    std::string combined = WideToUTF8(targetExePath) + "|" + WideToUTF8(args);
    std::string hash = GetPathHash(combined);
    std::wstring path = ManagedSooDir + L"\\" + UTF8ToWide(hash) + L".soo";
    return path;
}

// 检查文件是否存在
bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
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

    // .soo 扩展名注册
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
    if (!root.isMember("path") || !root.isMember("currentStartTime") || !root.isMember("preventTime")) return false;
    if (!root["path"].isString() || !root["currentStartTime"].isInt() || !root["preventTime"].isInt()) return false;
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
    std::wstring finalLnkPath;

    if (FileExists(baseLnkPath)) {
        if (overwrite) {
            finalLnkPath = baseLnkPath;
            DeleteFileW(finalLnkPath.c_str());
        }
        else {
            MessageBoxW(hwndOwner, L"快捷方式文件已存在，请勾选“覆盖”或修改文件名。", L"文件已存在", MB_ICONWARNING);
            return false;
        }
    }
    else {
        finalLnkPath = baseLnkPath;
    }

    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    std::wstring args = L"\"" + sooPath + L"\"";
    return CreateShortcutWithIcon(finalLnkPath, selfPath, args, targetExe);
}

bool CreateFree(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner) {
    std::wstring sooFileName = userFileName + L".soo";
    std::wstring baseSooPath = defaultFolder + L"\\" + sooFileName;
    std::wstring finalSooPath;

    if (FileExists(baseSooPath)) {
        if (overwrite) {
            finalSooPath = baseSooPath;
            DeleteFileW(finalSooPath.c_str());
        }
        else {
            MessageBoxW(hwndOwner, L"SOO 文件已存在，请勾选“覆盖”或修改文件名。", L"文件已存在", MB_ICONWARNING);
            return false;
        }
    }
    else {
        finalSooPath = baseSooPath;
    }

    CreateSoo(finalSooPath, targetExe, userArgs, 0, cooldown);
    std::ifstream test(WideToUTF8(finalSooPath));
    bool success = test.is_open();
    test.close();
    if (!success) {
        wchar_t msg[512];
        swprintf(msg, 512, L"创建 SOO 文件失败：\n%s\n\n可能原因：\n- 文件夹没有写入权限\n- 磁盘已满\n- 路径非法", finalSooPath.c_str());
        MessageBoxW(hwndOwner, msg, L"自由模式错误", MB_ICONERROR);
    }
    return success;
}

INT_PTR CALLBACK WizardProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        RECT rcDlg;
        GetWindowRect(hDlg, &rcDlg);
        int dlgWidth = rcDlg.right - rcDlg.left;
        int dlgHeight = rcDlg.bottom - rcDlg.top;
        RECT rcScreen;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int x = rcScreen.left + (rcScreen.right - rcScreen.left - dlgWidth) / 2;
        int y = rcScreen.top + (rcScreen.bottom - rcScreen.top - dlgHeight) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MAIN_ICON));
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

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
                std::wstring baseName = GetFileNameWithoutExt(filePath);
                SetDlgItemTextW(hDlg, IDC_NAME_EDIT, baseName.c_str());
            }
            return TRUE;
        }
        case IDOK: {
            wchar_t targetExe[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, IDC_TARGET_EDIT, targetExe, MAX_PATH);
            if (targetExe[0] == L'\0') {
                MessageBoxW(hDlg, L"请选择目标程序", L"提示", MB_ICONWARNING);
                return TRUE;
            }

            wchar_t fileName[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, IDC_NAME_EDIT, fileName, MAX_PATH);
            if (fileName[0] == L'\0') {
                std::wstring defaultName = GetFileNameWithoutExt(targetExe);
                wcscpy_s(fileName, MAX_PATH, defaultName.c_str());
            }

            wchar_t args[1024] = { 0 };
            GetDlgItemTextW(hDlg, IDC_ARGS_EDIT, args, 1024);

            BOOL overwrite = IsDlgButtonChecked(hDlg, IDC_OVERWRITE_CHECK) == BST_CHECKED;
            BOOL isManaged = IsDlgButtonChecked(hDlg, IDC_MODE_MANAGED) == BST_CHECKED;
            int cooldown = GetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, NULL, FALSE);
            if (cooldown < 0) cooldown = 0;

            bool success = false;
            if (isManaged) {
                success = CreateManaged(targetExe, args, fileName, cooldown, g_defaultFolder, overwrite, hDlg);
            }
            else {
                success = CreateFree(targetExe, args, fileName, cooldown, g_defaultFolder, overwrite, hDlg);
            }
            if (success) {
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
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_WIZARD),
        NULL, WizardProc, (LPARAM)folder.c_str());
    if (result == IDOK) {
        DeleteFileW(soogPath.c_str());
    }
}

//+++++++++++++++++++++++++
//小工具界面(未开工)
//+++++++++++++++++++++++++

void ToolMain() {
    MessageBoxW(NULL, L"StartOnlyOne工具模式", L"提示", MB_OK);
}

//+++++++++++++++++++++++++
//       处理命令行         +
//+++++++++++++++++++++++++

void CmdLinePros() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    if (argc == 1) {
        ToolMain();
        LocalFree(argv);
        return;
    }

    if (argc >= 3 && wcscmp(argv[1], L"/new") == 0) {
        Guide(argv[2]);
        LocalFree(argv);
        return;
    }

    if (argc == 2) {
        ReadSoo(argv[1]);
        LocalFree(argv);
        return;
    }

    LocalFree(argv);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    Init();
    CmdLinePros();
    return 0;
}