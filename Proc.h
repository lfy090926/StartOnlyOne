#ifndef PROC_H
#define PROC_H
#endif // !PROC_H

#include <Windows.h>
#include <string>
#include <vector>
#include "resource.h"
#include "common.h"
#include "FileOperation.h"
#include "SOO_Ex.h"
#include "Time.h"

static std::vector<std::wstring> g_backupList;   // 备份时间戳列表（供选择对话框使用）
static std::wstring g_selectedBackup;            // 用户选中的时间戳
static std::wstring g_defaultFolder;   // 用于在向导对话框间传递用户选择的文件夹路径


// 新建向导的对话框过程
INT_PTR CALLBACK WizardProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 备份选择对话框过程
INT_PTR CALLBACK SelectBackupProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// 转换桌面快捷方式引导对话框（允许用户输入冷却时间）
INT_PTR CALLBACK ConvertGuideProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// 备份并转换桌面快捷方式引导对话框
INT_PTR CALLBACK BackupConvertGuideProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

//关于对话框过程
INT_PTR CALLBACK AboutProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// 工具主界面过程
INT_PTR CALLBACK ToolMainProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
