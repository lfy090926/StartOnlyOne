#ifndef UI_P
#define UI_P
#endif // !UI_P

#include <string>
#include <Windows.h>
#include "GetPaths.h"
#include "resource.h"
#include "Proc.h"

// 向导入口：由 .soog 文件双击触发，显示向导，完成后删除临时 .soog 文件
void Guide(const std::wstring& soogPath);

// 工具主界面入口
void ToolMain();
