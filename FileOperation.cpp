#include <string>
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <vector>
#include "CharConvert.h"
#include "common.h"
#include "GetPaths.h"
#include "Time.h"


// 获取字符串的哈希值（用于生成 SOO 文件名）
// 参数: utf8Str - UTF-8 编码的字符串
// 返回: 十进制哈希字符串
std::string GetPathHash(const std::string& utf8Str) {
    std::hash<std::string> hasher;
    size_t hash = hasher(utf8Str);
    return std::to_string(hash);
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

// 创建指向 targetExe 的快捷方式，并设置图标为 iconSource 的第一个图标
// 参数:
//   lnkPath     - 要创建的 .lnk 文件完整路径
//   targetExe   - 快捷方式的目标程序（在本程序中通常是 SOO 文件路径）
//   arguments   - 命令行参数（为空则无参数）
//   iconSource  - 图标的来源文件（通常是目标 exe，也可能是 dll 等）
bool CreateShortcutWithIcon(const std::wstring& lnkPath, const std::wstring& targetExe,
    const std::wstring& arguments, const std::wstring& iconSource, int iconIndex) {
    CoInitialize(NULL);          // 初始化 COM（ShellLink 需要）
    bool success = false;
    IShellLinkW* psl = NULL;
    // 创建 ShellLink 对象
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetExe.c_str());
        psl->SetArguments(arguments.c_str());
        psl->SetIconLocation(iconSource.c_str(), iconIndex);   // 0 表示文件中的第一个图标
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

// 判断文件或目录是否存在（使用 std::ifstream，简单但可靠）
bool FileExists(const std::wstring& path) {
    std::ifstream f(path.c_str());
    return f.good();
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

