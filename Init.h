#ifndef INIT_H
#define INIT_H
#endif // !INIT_H

#include <Windows.h>
#include <ShlObj.h>
#include "common.h"
#include "FileOperation.h"

// 检查当前进程是否以管理员身份运行
BOOL IsRunningAsAdmin();

// 以管理员权限重新启动当前程序（会弹出 UAC 对话框）
void RunAsAdmin();

// 初始化程序所需的三个目录（根目录、soo_profiles、lnk_backup）
void InitDir();

// 辅助函数：向注册表写入一个字符串值（如果子键不存在则创建）
void SetRegString(HKEY root, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& data);

// 注册文件关联和右键新建菜单
// 1. 注册 .soog 扩展名，用于右键“新建”菜单触发向导（ShellNew + NullFile）
// 2. 注册 .soo 扩展名，使双击 .soo 文件时用本程序打开
// 3. 为 .soo 设置默认图标（本程序自带的图标）
void Regist();

// 卸载程序时清除注册表项（可选项）
void UnRegist();

// 初始化入口：创建目录 + 注册表
void Init();
