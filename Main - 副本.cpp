#define _CRT_SECURE_NO_WARNINGS

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

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

//+++++++++++++++++++++++++
//         初始化          +
//+++++++++++++++++++++++++

std::string RootDir;          // %APPDATA%\StartOnlyOne
std::string ManagedSooDir;    // %APPDATA%\StartOnlyOne\soo_profiles
std::string LnkBackupDir;     // %APPDATA%\StartOnlyOne\lnk_backup


//+++++++++++++++++++++++++
//        辅助函数         +
//+++++++++++++++++++++++++

//获取哈希值
std::string GetPathHash(const std::string& path) {
    std::hash<std::string> hasher;
    size_t hash = hasher(path);
    return std::to_string(hash);
}

//创建文件夹
void MakeDir(const char* path) {
    char tempPath[MAX_PATH] = { 0 };
    strcpy_s(tempPath, MAX_PATH, path);
    for (int i = 0; tempPath[i] != '\0'; i++) {
        if (tempPath[i] == '\\') {
            tempPath[i] = '\0';
            _mkdir(tempPath);
            tempPath[i] = '\\';
        }
    }
    _mkdir(tempPath);
}

//获取文件目录
void GetDirFromPath(const char* filePath, char* outDir)
{
    strncpy(outDir, filePath, MAX_PATH);

    // 从后往前找最后一个 \ 或 /
    char* p = strrchr(outDir, '\\');
    if (p == NULL) p = strrchr(outDir, '/');

    if (p != NULL) {
        *(p + 1) = '\0';  // 截断，得到文件夹
    }
}

//LPCWSTR转const char
char* WideToChar(LPCWSTR lpwStr)
{
    if (lpwStr == NULL) return _strdup("");

    // 计算需要的缓冲区大小
    int len = WideCharToMultiByte(CP_UTF8, 0, lpwStr, -1, NULL, 0, NULL, NULL);

    // 分配内存
    char* buffer = (char*)malloc(len);
    if (buffer == NULL) return _strdup("");

    // 执行转换
    WideCharToMultiByte(CP_UTF8, 0, lpwStr, -1, buffer, len, NULL, NULL);
    return buffer;
}

// 创建快捷方式并指定图标来源
bool CreateShortcutWithIcon(const char* lnkPath, const char* targetExe, const char* arguments, const char* iconSource) {
    // 初始化 COM 库（Shell API 需要）
    CoInitialize(NULL);
    bool success = false;

    // 创建 IShellLinkA 接口对象（ANSI 版本，直接处理 const char* 路径）
    IShellLinkA* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink,          // ShellLink 的 CLSID
        NULL,                     // 无聚合
        CLSCTX_INPROC_SERVER,     // 进程内服务器
        IID_IShellLinkA,          // 请求 ANSI 接口
        (void**)&psl);
    if (SUCCEEDED(hr)) {
        // 设置快捷方式的目标程序路径
        psl->SetPath(targetExe);
        // 设置启动参数
        psl->SetArguments(arguments);
        // 设置图标：iconSource 路径 + 图标索引（0 表示第一个图标）
        psl->SetIconLocation(iconSource, 0);

        // 将 ShellLink 对象保存为文件（需要 IPersistFile 接口）
        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            // IPersistFile::Save 要求宽字符路径，因此转换 lnkPath
            wchar_t wLnkPath[MAX_PATH];
            MultiByteToWideChar(CP_UTF8,             // 源编码 UTF-8
                0,                   // 默认标志
                lnkPath,             // 输入窄字符串
                -1,                  // 自动计算长度
                wLnkPath,            // 输出宽字符缓冲区
                MAX_PATH);           // 缓冲区大小
// 保存快捷方式文件（TRUE 表示文件名已包含路径）
            hr = ppf->Save(wLnkPath, TRUE);
            success = SUCCEEDED(hr);
            ppf->Release();   // 释放 IPersistFile
        }
        psl->Release();       // 释放 IShellLinkA
    }
    CoUninitialize();         // 卸载 COM 库
    return success;
}

// 从完整路径提取文件名（不含扩展名）
// 例如：C:\Program Files\QQ\QQ.exe -> "QQ"
std::string GetFileNameWithoutExt(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("\\/");
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    if (dot != std::string::npos && dot > start) {
        return path.substr(start, dot - start);
    }
    return path.substr(start);
}

// SOO文件名的计算（包含参数）
std::string GetSOOPath(const std::string& targetExePath, const std::string& args) {
    std::string combined = targetExePath + "|" + args;   // 用分隔符避免歧义
    std::string hash = GetPathHash(combined);
    std::string path = ManagedSooDir + "\\" + hash + ".soo";
    return path;
}

// 检查指定路径的文件是否存在（不包括目录）
// 参数：
//   path : 要检查的文件完整路径（例如 "C:\Desktop\QQ.lnk"）
// 返回值：如果文件存在且不是目录，返回 true；否则返回 false
bool FileExists(const std::string& path) {
    // 获取文件属性
    DWORD attr = GetFileAttributesA(path.c_str());
    // 如果属性无效（文件不存在）或者路径指向一个目录，则返回 false
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

// 生成一个不冲突的文件路径：如果原路径已存在，则在文件名后添加 (i) 后缀，直到找到一个不存在的路径
// 参数：
//   originalPath : 用户想要的原始文件路径（例如 "C:\Desktop\QQ.lnk"）
// 返回值：一个当前不存在的文件路径（例如 "C:\Desktop\QQ (2).lnk" 或 "C:\Desktop\QQ (3).lnk"）
// 注意：不会创建文件，只生成路径字符串
std::string GetUniqueFilePath(const std::string& originalPath) {
    // 如果原始路径不存在，直接返回
    if (!FileExists(originalPath)) {
        return originalPath;
    }

    // 分离目录、基础文件名和扩展名
    // 找到最后一个反斜杠或正斜杠的位置
    size_t lastSlash = originalPath.find_last_of("\\/");
    // 目录部分：如果找到斜杠则包含斜杠，否则为空（表示当前目录）
    std::string dir = (lastSlash == std::string::npos) ? "" : originalPath.substr(0, lastSlash + 1);
    // 完整文件名（含扩展名）
    std::string fullName = originalPath.substr(lastSlash + 1);

    // 分离文件名主体和扩展名
    size_t lastDot = fullName.find_last_of('.');
    std::string baseName = (lastDot == std::string::npos) ? fullName : fullName.substr(0, lastDot);
    std::string ext = (lastDot == std::string::npos) ? "" : fullName.substr(lastDot);

    // 从 2 开始尝试，依次生成 " (2)"、" (3)" 等后缀
    for (int i = 2; ; ++i) {
        // 构造新路径：目录 + 基础名 + 空格 + (序号) + 扩展名
        std::string newPath = dir + baseName + " (" + std::to_string(i) + ")" + ext;
        // 如果该路径不存在，则返回；否则继续尝试下一个序号
        if (!FileExists(newPath)) {
            return newPath;
        }
    }
}

// 弹出保存文件对话框，让用户选择保存路径（支持覆盖确认）
// 参数：
//   hwndOwner : 父窗口句柄（可为 NULL）
//   defaultPath: 默认的完整路径（包含文件名）
//   filter    : 文件类型过滤字符串（例如 "快捷方式\0*.lnk\0所有文件\0*.*\0"）
// 返回值：用户选择的文件路径（如果用户取消则返回空字符串）
std::string PromptSaveFile(HWND hwndOwner, const std::string& defaultPath, const char* filter) {
    OPENFILENAMEA ofn = { 0 };
    char fileBuf[MAX_PATH] = { 0 };
    // 复制默认路径到缓冲区
    strncpy(fileBuf, defaultPath.c_str(), MAX_PATH - 1);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;  // 自动覆盖提示
    if (GetSaveFileNameA(&ofn)) {
        return std::string(fileBuf);
    }
    return "";  // 用户取消
}

    //+++++++++++++++++++++++++
    //       工作目录创建       +
    //+++++++++++++++++++++++++

void InitDir() {
    char AppData[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, AppData);
    RootDir = std::string(AppData) + "\\StartOnlyOne";
    ManagedSooDir = RootDir + "\\soo_profiles";
    LnkBackupDir = RootDir + "\\lnk_backup";
    MakeDir(RootDir.c_str());
    MakeDir(ManagedSooDir.c_str());
    MakeDir(LnkBackupDir.c_str());

}

void UnInitDir() {

}

//+++++++++++++++++++++++++
//       注册表的注册       +
//+++++++++++++++++++++++++

void SetRegString(HKEY root, const char* subKey, const char* valueName, const char* data) {
    HKEY hKey;
    if (RegCreateKeyExA(root, subKey, 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, valueName, 0, REG_SZ, (const BYTE*)data, (DWORD)(strlen(data) + 1));
        RegCloseKey(hKey);
    }
}
#if 0
void Regist() {
    const char displayName[] = "SOO向导(双击启动向导)";
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string cmdLine = std::string("\"") + exePath + std::string("\" /new \"%1\"");

    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\.soog", NULL, "StartOnlyOneGuide");
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOneGuide", NULL, displayName);
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\.soog\\ShellNew", "NullFile", "");
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOneGuide\\shell\\open\\command", NULL, cmdLine.c_str());

}
#endif
void Regist() {
    // ========== 第一部分：注册 .soog 扩展名 (用于新建向导) ==========
    const char displayNameForGuide[] = "SOO向导(双击启动向导)";
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string cmdLine = std::string("\"") + exePath + std::string("\" /new \"%1\"");

    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\.soog", NULL, "StartOnlyOneGuide");
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOneGuide", NULL, displayNameForGuide);
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\.soog\\ShellNew", "NullFile", "");
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOneGuide\\shell\\open\\command", NULL, cmdLine.c_str());

    // ========== 第二部分：注册 .soo 扩展名并添加图标 (新增) ==========
    // 步骤 1: 定义 .soo 文件扩展名指向你的 ProgID
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\.soo", NULL, "StartOnlyOne.soo");

    // 步骤 2: 为 ProgID 设置默认打开命令 (使用你的程序)
    std::string openCmdLine = std::string("\"") + exePath + std::string("\" \"%1\"");
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOne.soo\\shell\\open\\command", NULL, openCmdLine.c_str());

    // 步骤 3: 为 ProgID 设置图标
    // 使用本程序自带的图标，索引为 0 (通常是你的主图标)
    std::string iconPath = std::string("\"") + exePath + "\",0";
    SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOne.soo\\DefaultIcon", NULL, iconPath.c_str());

    // --- 如果你希望使用独立的 .ico 文件，可以将上面两行替换为以下代码 ---
    // 假设你将一个 my_icon.ico 文件放在了程序所在目录
    // std::string iconPathForIco = std::string("\"") + exePathDir + "\\my_icon.ico\"";
    // SetRegString(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOne.soo\\DefaultIcon", NULL, iconPathForIco.c_str());
}

void UnRegist() {
    // 删除之前注册的 .soog 相关记录
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Classes\\.soog");
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOneGuide");

    // 新增：删除 .soo 扩展名相关注册 (清理干净)
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Classes\\.soo");
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Classes\\StartOnlyOne.soo");
}

void Init() {
    InitDir();
    Regist();
}

//+++++++++++++++++++++++++
//       SOO核心逻辑       +
//+++++++++++++++++++++++++

//文件是否有效
bool IsSooFileValid(const char* filePath)
{
    // 1. 打开文件
    std::ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        // 文件不存在 / 权限不足 / 路径错误
        return false;
    }

    // 2. 判断文件是否为空
    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    if (fileSize == 0)
    {
        ifs.close();
        return false;
    }

    // 3. 尝试解析JSON，解析失败 = 文件损坏
    Json::Reader reader;
    Json::Value root;
    bool parseOk = reader.parse(ifs, root);
    ifs.close();

    if (!parseOk)
    {
        // JSON 格式错误、乱码、截断、损坏
        return false;
    }

    // 4. 校验必要字段（防止JSON完好但内容残缺）
    if (!root.isObject())
        return false;

    if (!root.isMember("path") || !root.isMember("currentStartTime") || !root.isMember("preventTime"))
        return false;

    // 可选：校验字段类型
    if (!root["path"].isString() || !root["currentStartTime"].isInt() || !root["preventTime"].isInt())
        return false;

    return true;
}

//创建SOO文件
void CreateSoo(const char* sooFileName, const char* path, const char* args, int currentStartTime, int preventTime) {
    Json::Value soo;
    soo["path"] = path;
    soo["args"] = args;
    soo["currentStartTime"] = currentStartTime;
    soo["preventTime"] = preventTime;

    Json::StyledWriter writer;
    std::string file = writer.write(soo);
    std::ofstream ofs(sooFileName);
    ofs << file;
    ofs.close();
}

//读取并启动SOO文件
void ReadSoo(const char* file) {
    if (!IsSooFileValid(file)) {
        MessageBox(NULL, _T("文件可能损坏或无读取权限!"), _T("警告"), MB_ICONWARNING | MB_OK);
        return;
    }
    std::ifstream ifs(file);
    Json::Reader rd;
    Json::Value root;
    rd.parse(ifs, root);
    ifs.close();
    const char* path = root["path"].asCString();
    long long currentStartTime = root["currentStartTime"].asInt64();
    int preventTime = root["preventTime"].asInt();
    std::string args = root["args"].asString();
    std::time_t CurrentTime = std::time(nullptr);
    CreateSoo(file, path, args.c_str(), (long long)CurrentTime, preventTime);
    if (CurrentTime - currentStartTime < preventTime) {
        return;
    }
    std::wstring wpath = std::wstring(path, path + strlen(path));
    // ShellExecute 需要宽字符串参数
    std::wstring wargs = std::wstring(args.begin(), args.end());
    ShellExecute(NULL, L"open", wpath.c_str(), wargs.empty() ? NULL : wargs.c_str(), NULL, SW_SHOWNORMAL);
}

//+++++++++++++++++++++++++
//          引导           +
//+++++++++++++++++++++++++

static char g_defaultFolder[MAX_PATH];

// 托管模式：生成哈希命名的 .soo 文件（位于 ManagedSooDir），并在 defaultFolder 中创建快捷方式
bool CreateManaged(const char* targetExe, const char* userArgs, const char* userFileName,
    int cooldown, const char* defaultFolder, bool overwrite, HWND hwndOwner) {
    // 1. 计算 SOO 文件路径（哈希命名，固定位置）
    std::string sooPath = GetSOOPath(targetExe, userArgs);
    // 创建 SOO 文件（附加参数保存到 JSON）
    CreateSoo(sooPath.c_str(), targetExe, userArgs, 0, cooldown);

    // 2. 构建快捷方式的基础路径
    std::string lnkFileName = std::string(userFileName) + ".lnk";
    std::string baseLnkPath = std::string(defaultFolder) + "\\" + lnkFileName;
    std::string finalLnkPath;

    // 3. 处理文件名冲突
    if (FileExists(baseLnkPath)) {
        if (overwrite) {
            finalLnkPath = baseLnkPath;
            // 如果存在且覆盖，先删除旧文件（避免覆盖权限问题）
            DeleteFileA(finalLnkPath.c_str());
        }
        else {
            // 弹出另存为对话框，让用户自己选择
            std::string filter = "快捷方式\0*.lnk\0所有文件\0*.*\0";
            std::string newPath = PromptSaveFile(hwndOwner, baseLnkPath, filter.c_str());
            if (newPath.empty()) {
                return false;   // 用户取消
            }
            finalLnkPath = newPath;
        }
    }
    else {
        finalLnkPath = baseLnkPath;
    }

    // 4. 获取本程序自身路径
    char selfPath[MAX_PATH];
    GetModuleFileNameA(NULL, selfPath, MAX_PATH);

    // 5. 构建快捷方式的命令行参数
    std::string args = std::string("\"") + sooPath + "\"";

    // 6. 创建快捷方式
    return CreateShortcutWithIcon(finalLnkPath.c_str(), selfPath, args.c_str(), targetExe);
}// 自由模式：不弹窗，直接在 defaultFolder 中创建 .soo 文件（不创建快捷方式）

// 自由模式：在 defaultFolder 中创建 .soo 文件（不创建快捷方式）
bool CreateFree(const char* targetExe, const char* userArgs, const char* userFileName,
    int cooldown, const char* defaultFolder, bool overwrite, HWND hwndOwner) {
    // 1. 构造 .soo 文件基础路径
    std::string sooFileName = std::string(userFileName) + ".soo";
    std::string baseSooPath = std::string(defaultFolder) + "\\" + sooFileName;
    std::string finalSooPath;

    // 2. 处理文件名冲突
    if (FileExists(baseSooPath)) {
        if (overwrite) {
            finalSooPath = baseSooPath;
            DeleteFileA(finalSooPath.c_str());
        }
        else {
            std::string filter = "SOO文件\0*.soo\0所有文件\0*.*\0";
            std::string newPath = PromptSaveFile(hwndOwner, baseSooPath, filter.c_str());
            if (newPath.empty()) {
                return false;
            }
            finalSooPath = newPath;
        }
    }
    else {
        finalSooPath = baseSooPath;
    }

    // 3. 创建 SOO 文件
    CreateSoo(finalSooPath.c_str(), targetExe, userArgs, 0, cooldown);

    // 4. 验证文件是否创建成功
    std::ifstream test(finalSooPath);
    bool success = test.is_open();
    test.close();

    if (!success) {
        char msg[512];
        sprintf(msg, "创建 SOO 文件失败：\n%s\n\n可能原因：\n- 文件夹没有写入权限\n- 磁盘已满\n- 路径非法",
            finalSooPath.c_str());
        MessageBoxA(hwndOwner, msg, "自由模式错误", MB_ICONERROR);
    }
    return success;
}

INT_PTR CALLBACK WizardProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        // 获取对话框大小
        RECT rcDlg;
        GetWindowRect(hDlg, &rcDlg);
        int dlgWidth = rcDlg.right - rcDlg.left;
        int dlgHeight = rcDlg.bottom - rcDlg.top;

        // 获取屏幕大小（工作区，即不包括任务栏）
        RECT rcScreen;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int screenWidth = rcScreen.right - rcScreen.left;
        int screenHeight = rcScreen.bottom - rcScreen.top;

        // 计算居中位置
        int x = rcScreen.left + (screenWidth - dlgWidth) / 2;
        int y = rcScreen.top + (screenHeight - dlgHeight) / 2;

        // 移动窗口
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        // 获取当前模块的实例句柄
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);
        // 加载图标资源
        HICON hIconSmall = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
        HICON hIconBig = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
        // 设置标题栏小图标和任务栏图标（ICON_SMALL 为 16x16，ICON_BIG 为 32x32）
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
        // 接收传入的默认文件夹路径
        if (lParam) {
            strncpy(g_defaultFolder, (char*)lParam, MAX_PATH);
        }
        // 默认选中“托管模式”
        CheckRadioButton(hDlg, IDC_MODE_MANAGED, IDC_MODE_FREE, IDC_MODE_MANAGED);
        // 默认冷却时间 5 秒
        SetDlgItemInt(hDlg, IDC_COOLDOWN_EDIT, 5, FALSE);
        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BROWSE_BTN: {
            OPENFILENAMEA ofn = { 0 };
            char filePath[MAX_PATH] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = "可执行文件\0*.exe\0所有文件\0*.*\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameA(&ofn)) {
                SetDlgItemTextA(hDlg, IDC_TARGET_EDIT, filePath);

                // 自动填入文件名（不含扩展名）
                std::string exePath = filePath;
                std::string baseName = GetFileNameWithoutExt(exePath);
                SetDlgItemTextA(hDlg, IDC_NAME_EDIT, baseName.c_str());
            }
            return TRUE;
        }
        case IDOK: {
            char targetExe[MAX_PATH] = { 0 };
            GetDlgItemTextA(hDlg, IDC_TARGET_EDIT, targetExe, MAX_PATH);
            if (targetExe[0] == '\0') {
                MessageBoxA(hDlg, "请选择目标程序", "提示", MB_ICONWARNING);
                return TRUE;
            }

            // 获取用户输入的文件名（不含扩展名）
            char fileName[MAX_PATH] = { 0 };
            GetDlgItemTextA(hDlg, IDC_NAME_EDIT, fileName, MAX_PATH);
            if (fileName[0] == '\0') {
                // 如果没有输入，默认使用目标程序的文件名（不含扩展名）
                std::string defaultName = GetFileNameWithoutExt(targetExe);
                strncpy(fileName, defaultName.c_str(), MAX_PATH - 1);
            }

            // 获取附加参数
            char args[1024] = { 0 };
            GetDlgItemTextA(hDlg, IDC_ARGS_EDIT, args, 1024);

            // 获取覆盖复选框状态
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
            else {
                MessageBoxA(hDlg, "创建失败，请检查目录权限或路径", "错误", MB_ICONERROR);
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
void Guide(LPWSTR soogPath) {
    // 将临时 .soog 文件路径转换为窄字符串（用于提取目录）
    char* pathUtf8 = WideToChar(soogPath);
    if (!pathUtf8) return;

    // 提取文件所在目录
    char folder[MAX_PATH];
    GetDirFromPath(pathUtf8, folder);

    // 显示对话框，传入文件夹路径作为参数
    INT_PTR result = DialogBoxParamA(GetModuleHandle(NULL),
        MAKEINTRESOURCEA(IDD_WIZARD),
        NULL,
        WizardProc,
        (LPARAM)folder);
    if (result == IDOK) {
        // 向导成功完成，删除临时 .soog 文件
        DeleteFileW(soogPath);
    }

    free(pathUtf8);
}



//+++++++++++++++++++++++++
//小工具界面(未开工)
//+++++++++++++++++++++++++

//占位弹窗(可删)
void ToolMain() {
    MessageBox(NULL, _T("StartOnlyOne工具模式"), _T("提示"), MB_OK);
}

//+++++++++++++++++++++++++
//       处理命令行         +
//+++++++++++++++++++++++++
#if 0
void CmdLinePros() {
    int argc;
    LPWSTR* argv;
    LPWSTR fullCmdLine = GetCommandLineW();
    argv = CommandLineToArgvW(fullCmdLine, &argc);
    if (argc == 1) {
        ToolMain();
        return;
    }
    if (argc == 2) {
        ReadSoo(WideToChar(argv[2]));
    }
    if (argv[1] == LPWSTR("/new")) {
        Guide(argv[2]);
    }

}
#endif
void CmdLinePros() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    if (argc == 1) {
        ToolMain();
        LocalFree(argv);
        return;
    }

    // 处理 /new 参数（注意：比较字符串内容）
    if (argc >= 3 && wcscmp(argv[1], L"/new") == 0) {
        // argv[2] 是临时 .soog 文件的完整路径
        Guide(argv[2]);   // 传入 LPWSTR
        LocalFree(argv);
        return;
    }

    // 单个参数：可能是 .soo 文件路径
    if (argc == 2) {
        char* sooPath = WideToChar(argv[1]);
        ReadSoo(sooPath);
        free(sooPath);   // 释放内存
        LocalFree(argv);
        return;
    }

    LocalFree(argv);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nShowCmd) {
    Init();
    CmdLinePros();
    return 0;
}
