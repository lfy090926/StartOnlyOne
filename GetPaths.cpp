#include <string>
#include <vector>
#include <Windows.h>
#include <ShlObj.h>
#include "common.h"
#include "CharConvert.h"
#include "FileOperation.h"


// 需要备份的根目录列表（CSIDL 常量）：当前用户桌面、公共桌面
// 注意：程序启动时会读取此列表，未来可扩展其他位置（如任务栏、启动文件夹）
std::vector<int> BakRoots = { CSIDL_DESKTOP, CSIDL_COMMON_DESKTOPDIRECTORY };

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

