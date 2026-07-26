#ifndef TIME_H
#define TIME_H
#endif // !TIME_H

#include <string>
#include <ctime>
#include <vector>
#include <Windows.h>
#include <algorithm>
#include "CharConvert.h"

// 获取当前时间的 Unix 时间戳（秒数），作为备份文件夹的名称
std::wstring GetCurrentTimestampStr();

// 将 Unix 时间戳字符串转换为本地日期时间字符串（用于界面显示）
std::wstring TimestampToDisplay(const std::wstring& timestamp);

// 获取备份根目录下的所有时间戳子目录（文件夹名就是时间戳），并按时间倒序排序（最新的在前）
std::vector<std::wstring> GetBackupTimestamps(const std::wstring& backupDir);
