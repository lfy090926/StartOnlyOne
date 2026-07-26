#include <string>
#include <ctime>
#include <vector>
#include <Windows.h>
#include <algorithm>
#include "CharConvert.h"

// 获取当前时间的 Unix 时间戳（秒数），作为备份文件夹的名称
std::wstring GetCurrentTimestampStr() {
    std::time_t CurrentTime = std::time(nullptr);
    wchar_t buf[64];
    swprintf_s(buf, 64, L"%lld", (long long)CurrentTime);
    return std::wstring(buf);
}

// 将 Unix 时间戳字符串转换为本地日期时间字符串（用于界面显示）
std::wstring TimestampToDisplay(const std::wstring& timestamp) {
    std::string sTimeStamp = WideToUTF8(timestamp);
    std::time_t CurrentTime = std::stoll(sTimeStamp);
    struct tm LocalTime;
    localtime_s(&LocalTime, &CurrentTime);
    int year = LocalTime.tm_year + 1900,
        month = LocalTime.tm_mon + 1,
        day = LocalTime.tm_mday,
        hour = LocalTime.tm_hour,
        minute = LocalTime.tm_min,
        second = LocalTime.tm_sec;
    wchar_t buf[64];
    swprintf_s(buf, 64, L"%04d年%02d月%02d日 %02d:%02d:%02d",
        year, month, day, hour, minute, second);
    return std::wstring(buf);
}

// 获取备份根目录下的所有时间戳子目录（文件夹名就是时间戳），并按时间倒序排序（最新的在前）
std::vector<std::wstring> GetBackupTimestamps(const std::wstring& backupDir) {
    std::vector<std::wstring> timestamps;
    std::wstring searchPath = backupDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return timestamps;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                timestamps.push_back(fd.cFileName);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    std::sort(timestamps.begin(), timestamps.end(), std::greater<std::wstring>());
    return timestamps;
}
