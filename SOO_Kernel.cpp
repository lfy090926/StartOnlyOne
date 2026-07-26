#include <string>
#include <fstream>
#include <json/json.h>
#include "CharConvert.h"

// 检查 SOO 文件是否有效：存在、非空、JSON 格式正确、包含必要字段
bool IsSooFileValid(const std::wstring& filePath) {
    std::ifstream ifs(WideToUTF8(filePath));
    if (!ifs.is_open()) return false;
    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    if (fileSize == 0) { ifs.close(); return false; }
    Json::Reader reader;
    Json::Value root;
    bool parseOk = reader.parse(ifs, root);
    ifs.close();
    if (!parseOk) return false;
    if (!root.isObject()) return false;
    if (!root.isMember("path") || !root.isMember("currentStartTime") || !root.isMember("preventTime") || !root.isMember("args")) return false;
    if (!root["path"].isString() || !root["currentStartTime"].isInt() || !root["preventTime"].isInt() || !root["args"].isString()) return false;
    return true;
}

// 创建 SOO 文件，内容为 JSON 格式
void CreateSoo(const std::wstring& sooFileName, const std::wstring& path, const std::wstring& args,
    long long currentStartTime, int preventTime) {
    Json::Value soo;
    soo["path"] = WideToUTF8(path);
    soo["args"] = WideToUTF8(args);
    soo["currentStartTime"] = (Json::Int64)currentStartTime;
    soo["preventTime"] = preventTime;
    Json::StyledWriter writer;
    std::string file = writer.write(soo);
    std::ofstream ofs(WideToUTF8(sooFileName));
    ofs << file;
    ofs.close();
}

// 读取 SOO 文件，检查冷却时间并启动目标程序
void ReadSoo(const std::wstring& file) {
    if (!IsSooFileValid(file)) {
        MessageBoxW(NULL, L"文件可能损坏或无读取权限!", L"警告", MB_ICONWARNING | MB_OK);
        return;
    }
    std::ifstream ifs(WideToUTF8(file));
    Json::Reader rd;
    Json::Value root;
    rd.parse(ifs, root);
    ifs.close();
    std::wstring path = UTF8ToWide(root["path"].asString());
    long long currentStartTime = root["currentStartTime"].asInt64();
    int preventTime = root["preventTime"].asInt();
    std::wstring args = UTF8ToWide(root["args"].asString());
    std::time_t CurrentTime = std::time(nullptr);
    // 重要：先更新 SOO 文件中的 currentStartTime 为当前时间（即使不启动也要更新，防止短时间频繁请求）
    CreateSoo(file, path, args, (long long)CurrentTime, preventTime);
    // 冷却判断：如果距离上次启动时间不足 preventTime 秒，则不启动
    if (CurrentTime - currentStartTime < preventTime) return;
    ShellExecuteW(NULL, L"open", path.c_str(), args.empty() ? NULL : args.c_str(), NULL, SW_SHOWNORMAL);
}
