#include <string>
#include <shlobj.h>

// UTF-8 字符串转宽字符串（Windows API 大多使用 UTF-16 宽字符）
std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    // 计算所需缓冲区大小（包括结尾的 L'\0'）
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    std::wstring result(len - 1, 0);           // 减去结尾的 L'\0'
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

// 宽字符串转 UTF-8（用于 JSON 序列化，便于保存非 ASCII 字符）
std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}