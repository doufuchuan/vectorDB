#pragma once

#include "scalar_storage.h"
#include "index_factory.h"
#include <string>
#include <vector>
#include <rapidjson/document.h>

class VectorDatabase {
public:
    // 构造函数
    VectorDatabase(const std::string& db_path);

    // 插入或更新向量
    void upsert(uint64_t id, const rapidjson::Document& data, IndexFactory::IndexType index_type);
    rapidjson::Document query(uint64_t id); // 添加query接口
    std::pair<std::vector<long>, std::vector<float>> search(const rapidjson::Document& json_request); // 添加 search 方法声明

private:
    ScalarStorage scalar_storage_;
};