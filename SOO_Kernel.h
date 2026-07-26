#ifndef SOO_KERNEL_H
#define SOO_KERNEL_H
#endif // !SOO_KERNEL_H

#include <string>
#include <fstream>
#include <json/json.h>
#include "CharConvert.h"

// 检查 SOO 文件是否有效：存在、非空、JSON 格式正确、包含必要字段
bool IsSooFileValid(const std::wstring& filePath);

// 创建 SOO 文件，内容为 JSON 格式
void CreateSoo(const std::wstring& sooFileName, const std::wstring& path, const std::wstring& args,
    long long currentStartTime, int preventTime);

// 读取 SOO 文件，检查冷却时间并启动目标程序
void ReadSoo(const std::wstring& file);