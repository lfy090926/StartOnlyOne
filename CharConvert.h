#ifndef CHARCONVERT_H
#define CHARCONVERT_H
#endif

#include <string>
#include <shlobj.h>

// UTF-8 字符串转宽字符串（Windows API 大多使用 UTF-16 宽字符）
std::wstring UTF8ToWide(const std::string& utf8);

// 宽字符串转 UTF-8（用于 JSON 序列化，便于保存非 ASCII 字符）
std::string WideToUTF8(const std::wstring& wstr);