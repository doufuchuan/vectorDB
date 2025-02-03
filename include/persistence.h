#pragma once

#include <string>
#include <fstream>
#include <cstdint> // 包含 <cstdint> 以使用 uint64_t 类型
#include <rapidjson/document.h> // 包含 rapidjson/document.h 以使用 JSON 对象

class Persistence {
public:
    Persistence();
    ~Persistence();

    void init(const std::string& local_path); // 添加 init 方法声明
    uint64_t increaseID();
    uint64_t getID() const;
    void writeWALLog(const std::string& operation_type, const rapidjson::Document& json_data, const std::string& version); // 添加 version 参数
    void readNextWALLog(std::string* operation_type, rapidjson::Document* json_data); // 更改返回类型为 void 并添加指针参数

private:
    uint64_t increaseID_;
    std::fstream wal_log_file_; // 将 wal_log_file_ 类型更改为 std::fstream
};