#include <string>
#include <Windows.h>
#include "GetPaths.h"
#include "resource.h"
#include "Proc.h"

// 向导入口：由 .soog 文件双击触发，显示向导，完成后删除临时 .soog 文件
void Guide(const std::wstring& soogPath) {
    std::wstring folder;
    GetDirFromPath(soogPath, folder);
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_WIZARD), NULL, WizardProc, (LPARAM)folder.c_str());
    if (result == IDOK) DeleteFileW(soogPath.c_str());
}

// 工具主界面入口
void ToolMain() {
    DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_TOOLMAIN), NULL, ToolMainProc);
}
