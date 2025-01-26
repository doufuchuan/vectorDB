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

private:
    ScalarStorage scalar_storage_;
};