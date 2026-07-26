#include <string>
#include <ShlObj.h>
#include "GetPaths.h"
#include "SOO_Kernel.h"

// 托管模式：创建 SOO 文件，并在 defaultFolder 下生成一个指向该 SOO 的快捷方式
// 快捷方式的图标取自 targetExe
bool CreateManaged(const std::wstring& targetExe, const std::wstring& userArgs, const std::wstring& userFileName,
    int cooldown, const std::wstring& defaultFolder, bool overwrite, HWND hwndOwner, std::wstring iconSource,
    int iconIndex) {
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
    return CreateShortcutWithIcon(baseLnkPath, sooPath, L"", iconSource, iconIndex);
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
    wchar_t icon[MAX_PATH] = { 0 };
    int piIcon = 0;
    psl->GetPath(target, MAX_PATH, NULL, SLGP_UNCPRIORITY);
    psl->GetArguments(args, 1024);
    psl->GetIconLocation(icon, MAX_PATH, &piIcon);
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
    if (icon[1] != '\0') {
        return CreateManaged(targetStr, args, fileName, cooldown, folder, true, NULL, icon, piIcon);
    }
    return CreateManaged(targetStr, args, fileName, cooldown, folder, true, NULL, targetStr, 0);

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
    swprintf_s(msg, 256, L"转换完成：%d/%d 个快捷方式", cnt, (int)lnks.size());
    MessageBoxW(hParent, msg, L"结果", MB_OK);
}

