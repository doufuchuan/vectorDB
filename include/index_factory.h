#pragma once

#include "faiss_index.h"
#include "scalar_storage.h" // 包含 scalar_storage.h 以使用 ScalarStorage 类
#include <map>

class IndexFactory {
public:
    enum class IndexType {
        FLAT,
        HNSW,
        FILTER, // 添加 FILTER 枚举值
        UNKNOWN = -1 
    };

    enum class MetricType {
        L2,
        IP
    };

    //void init(IndexType type, int dim, MetricType metric = MetricType::L2);
    void init(IndexFactory::IndexType type, int dim = 1, int num_data = 0, IndexFactory::MetricType metric = IndexFactory::MetricType::L2);
    void* getIndex(IndexType type) const;
    void saveIndex(const std::string& folder_path, ScalarStorage& scalar_storage); // 添加 ScalarStorage 参数
    void loadIndex(const std::string& folder_path, ScalarStorage& scalar_storage); // 添加 loadIndex 方法声明

private:
    std::map<IndexType, void*> index_map; 
};

IndexFactory* getGlobalIndexFactory();