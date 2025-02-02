#include "constants.h"
#include "vector_database.h"
#include "scalar_storage.h"
#include "index_factory.h"
#include "faiss_index.h"
#include "hnswlib_index.h"
#include "filter_index.h" // 包含 filter_index.h 以使用 FilterIndex 类
#include "logger.h" 
#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h> // 包含 rapidjson/stringbuffer.h 以使用 StringBuffer 类
#include <rapidjson/writer.h> // 包含 rapidjson/writer.h 以使用 Writer 类
VectorDatabase::VectorDatabase(const std::string& db_path) : scalar_storage_(db_path) {}

void VectorDatabase::upsert(uint64_t id, const rapidjson::Document& data, IndexFactory::IndexType index_type) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    GlobalLogger->info("Upsert data: {}", buffer.GetString());

    // 检查标量存储中是否存在给定ID的向量
    rapidjson::Document existingData;
    try {
        existingData = scalar_storage_.get_scalar(id);
    } catch (const std::runtime_error& e) {
        // 向量不存在，继续执行插入操作
    }

    // 如果存在现有向量，则从索引中删除它
    if (existingData.IsObject()) {
        GlobalLogger->debug("try remove old index"); // 添加打印信息
        std::vector<float> existingVector(existingData["vectors"].Size());
        for (rapidjson::SizeType i = 0; i < existingData["vectors"].Size(); ++i) {
            existingVector[i] = existingData["vectors"][i].GetFloat();
        }

        void* index = getGlobalIndexFactory()->getIndex(index_type);
        switch (index_type) {
            case IndexFactory::IndexType::FLAT: {
                FaissIndex* faiss_index = static_cast<FaissIndex*>(index);
                faiss_index->remove_vectors({static_cast<long>(id)});
                break;
            }
            case IndexFactory::IndexType::HNSW: {
                HNSWLibIndex* hnsw_index = static_cast<HNSWLibIndex*>(index);
                //hnsw_index->remove_vectors({id});
                break;
            }
            default:
                break;
        }
    }

    // 将新向量插入索引
    std::vector<float> newVector(data["vectors"].Size());
    for (rapidjson::SizeType i = 0; i < data["vectors"].Size(); ++i) {
        newVector[i] = data["vectors"][i].GetFloat();
    }

    GlobalLogger->debug("try add new index"); // 添加打印信息

    void* index = getGlobalIndexFactory()->getIndex(index_type);
    switch (index_type) {
        case IndexFactory::IndexType::FLAT: {
            FaissIndex* faiss_index = static_cast<FaissIndex*>(index);
            faiss_index->insert_vectors(newVector, id);
            break;
        }
        case IndexFactory::IndexType::HNSW: {
            HNSWLibIndex* hnsw_index = static_cast<HNSWLibIndex*>(index);
            hnsw_index->insert_vectors(newVector, id);
            break;
        }
        default:
            break;
    }

    GlobalLogger->debug("try add new filter"); // 添加打印信息
    // 检查客户写入的数据中是否有 int 类型的 JSON 字段
    FilterIndex* filter_index = static_cast<FilterIndex*>(getGlobalIndexFactory()->getIndex(IndexFactory::IndexType::FILTER));
    for (auto it = data.MemberBegin(); it != data.MemberEnd(); ++it) {
        std::string field_name = it->name.GetString();
        GlobalLogger->debug("try filter member {} {}",it->value.IsInt(), field_name); // 添加打印信息
        if (it->value.IsInt() && field_name != "id") { // 过滤名称为 "id" 的字段
            int64_t field_value = it->value.GetInt64();

            int64_t* old_field_value_p = nullptr;
            // 如果存在现有向量，则从 FilterIndex 中更新 int 类型字段
            if (existingData.IsObject()) {
                old_field_value_p = (int64_t*)malloc(sizeof(int64_t));
                *old_field_value_p = existingData[field_name.c_str()].GetInt64();
            } 
            
            filter_index->updateIntFieldFilter(field_name, old_field_value_p, field_value, id);
            if(old_field_value_p) {
                delete old_field_value_p;
            }
        }
    }

    // 更新标量存储中的向量
    scalar_storage_.insert_scalar(id, data);
}


rapidjson::Document VectorDatabase::query(uint64_t id) { // 添加query函数实现
    return scalar_storage_.get_scalar(id);
}

std::pair<std::vector<long>, std::vector<float>> VectorDatabase::search(const rapidjson::Document& json_request) {
    // 从 JSON 请求中获取查询参数
    std::vector<float> query;
    for (const auto& q : json_request[REQUEST_VECTORS].GetArray()) {
        query.push_back(q.GetFloat());
    }
    int k = json_request[REQUEST_K].GetInt();

    // 获取请求参数中的索引类型
    IndexFactory::IndexType indexType = IndexFactory::IndexType::UNKNOWN;
    if (json_request.HasMember(REQUEST_INDEX_TYPE) && json_request[REQUEST_INDEX_TYPE].IsString()) {
        std::string index_type_str = json_request[REQUEST_INDEX_TYPE].GetString();
        if (index_type_str == INDEX_TYPE_FLAT) {
            indexType = IndexFactory::IndexType::FLAT;
        } else if (index_type_str == INDEX_TYPE_HNSW) {
            indexType = IndexFactory::IndexType::HNSW;
        }
    }

    // 检查请求中是否包含 filter 参数
    roaring_bitmap_t* filter_bitmap = nullptr;
    if (json_request.HasMember("filter") && json_request["filter"].IsObject()) {
        const auto& filter = json_request["filter"];
        std::string fieldName = filter["fieldName"].GetString();
        std::string op_str = filter["op"].GetString();
        int64_t value = filter["value"].GetInt64();

        FilterIndex::Operation op = (op_str == "=") ? FilterIndex::Operation::EQUAL : FilterIndex::Operation::NOT_EQUAL;

        // 通过 getGlobalIndexFactory 的 getIndex 方法获取 FilterIndex
        FilterIndex* filter_index = static_cast<FilterIndex*>(getGlobalIndexFactory()->getIndex(IndexFactory::IndexType::FILTER));

        // 调用 FilterIndex 的 getIntFieldFilterBitmap 方法
        filter_bitmap = roaring_bitmap_create();
        filter_index->getIntFieldFilterBitmap(fieldName, op, value, filter_bitmap);
    }

    // 使用全局 IndexFactory 获取索引对象
    void* index = getGlobalIndexFactory()->getIndex(indexType);

    // 根据索引类型初始化索引对象并调用 search_vectors 函数
    std::pair<std::vector<long>, std::vector<float>> results;
    switch (indexType) {
        case IndexFactory::IndexType::FLAT: {
            FaissIndex* faissIndex = static_cast<FaissIndex*>(index);
            results = faissIndex->search_vectors(query, k, filter_bitmap); // 将 filter_bitmap 传递给 search_vectors 方法
            break;
        }
        case IndexFactory::IndexType::HNSW: {
            HNSWLibIndex* hnswIndex = static_cast<HNSWLibIndex*>(index);
            results = hnswIndex->search_vectors(query, k, filter_bitmap); // 将 filter_bitmap 传递给 search_vectors 方法
            break;
        }
        // 在此处添加其他索引类型的处理逻辑
        default:
            break;
    }
    if (filter_bitmap != nullptr) {
        delete filter_bitmap;
    }
    return results;
}