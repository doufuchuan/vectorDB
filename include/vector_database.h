#pragma once

#include "scalar_storage.h"
#include "index_factory.h"
#include "persistence.h" // 包含 persistence.h 以使用 Persistence 类
// #include "raft_stuff.h"
#include <string>
#include <vector>
#include <rapidjson/document.h>

class VectorDatabase {
public:
    // 构造函数
    VectorDatabase(const std::string& db_path, const std::string& wal_path); // 添加 wal_path 参数

    // 插入或更新向量
    void upsert(uint64_t id, const rapidjson::Document& data, IndexFactory::IndexType index_type);
    rapidjson::Document query(uint64_t id); // 添加query接口
    std::pair<std::vector<long>, std::vector<float>> search(const rapidjson::Document& json_request); // 添加 search 方法声明
    void reloadDatabase(); // 添加 reloadDatabase 方法声明
    void writeWALLog(const std::string& operation_type, const rapidjson::Document& json_data); // 添加 writeWALLog 方法声明
    void writeWALLogWithID(uint64_t log_id, const std::string& data); // 添加 writeWALLogWithID 函数声明
    void takeSnapshot(); // 添加 takeSnapshot 方法声明
    IndexFactory::IndexType getIndexTypeFromRequest(const rapidjson::Document& json_request); // 将 getIndexTypeFromRequest 方法设为 public
    int64_t getStartIndexID() const; // 添加 getStartIndexID 函数声明

private:
    ScalarStorage scalar_storage_;
    Persistence persistence_; // 添加 Persistence 对象
};