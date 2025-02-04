#pragma once

#include <rocksdb/db.h>
#include <string>
#include <vector>
#include <rapidjson/document.h> // 包含rapidjson头文件

class ScalarStorage {
public:
    // 构造函数，打开RocksDB
    ScalarStorage(const std::string& db_path);

    // 析构函数，关闭RocksDB
    ~ScalarStorage();

    // 向量插入函数
    void insert_scalar(uint64_t id, const rapidjson::Document& data); // 将参数类型更改为rapidjson::Document

    // 根据ID查询向量函数
    rapidjson::Document get_scalar(uint64_t id); // 将返回类型更改为rapidjson::Document
    void put(const std::string& key, const std::string& value); // 添加 put 方法声明
    std::string get(const std::string& key); // 添加 get 方法声明

private:
    // RocksDB实例
    rocksdb::DB* db_;
};