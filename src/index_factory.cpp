#include "index_factory.h"
#include "hnswlib_index.h"
#include "filter_index.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <experimental/filesystem> // 包含 <experimental/filesystem> 以使用 std::experimental::filesystem

namespace {
    IndexFactory globalIndexFactory; 
}

IndexFactory* getGlobalIndexFactory() {
    return &globalIndexFactory; 
}

void IndexFactory::init(IndexFactory::IndexType type, int dim, int num_data, IndexFactory::MetricType metric) {
    faiss::MetricType faiss_metric = (metric == IndexFactory::MetricType::L2) ? faiss::METRIC_L2 : faiss::METRIC_INNER_PRODUCT;

    switch (type) {
        case IndexFactory::IndexType::FLAT:
            index_map[type] = new FaissIndex(new faiss::IndexIDMap(new faiss::IndexFlat(dim, faiss_metric)));
            break;
        case IndexFactory::IndexType::HNSW:
            index_map[type] = new HNSWLibIndex(dim, num_data, metric, 16, 200);
            break;
        case IndexFactory::IndexType::FILTER: // 初始化 FilterIndex 对象
            index_map[type] = new FilterIndex();
            break;
        default:
            break;
    }
}

void* IndexFactory::getIndex(IndexType type) const { 
    auto it = index_map.find(type);
    if (it != index_map.end()) {
        return it->second;
    }
    return nullptr;
}

void IndexFactory::saveIndex(const std::string& folder_path, ScalarStorage& scalar_storage) { // 添加 ScalarStorage 参数

    for (const auto& index_entry : index_map) {
        IndexType index_type = index_entry.first;
        void* index = index_entry.second;

        // 为每个索引类型生成一个文件名
        std::string file_path = folder_path + std::to_string(static_cast<int>(index_type)) + ".index";

        // 根据索引类型调用相应的 saveIndex 函数
        if (index_type == IndexType::FLAT) {
            static_cast<FaissIndex*>(index)->saveIndex(file_path);
        } else if (index_type == IndexType::HNSW) {
            static_cast<HNSWLibIndex*>(index)->saveIndex(file_path);
        } else if (index_type == IndexType::FILTER) { // 保存 FilterIndex 类型的索引
            static_cast<FilterIndex*>(index)->saveIndex(scalar_storage, file_path);
        }
    }
}

void IndexFactory::loadIndex(const std::string& folder_path, ScalarStorage& scalar_storage) { // 添加 loadIndex 方法实现
    for (const auto& index_entry : index_map) {
        IndexType index_type = index_entry.first;
        void* index = index_entry.second;

        // 为每个索引类型生成一个文件名
        std::string file_path = folder_path + std::to_string(static_cast<int>(index_type)) + ".index";

        // 根据索引类型调用相应的 loadIndex 函数
        if (index_type == IndexType::FLAT) {
            static_cast<FaissIndex*>(index)->loadIndex(file_path);
        } else if (index_type == IndexType::HNSW) {
            static_cast<HNSWLibIndex*>(index)->loadIndex(file_path);
        } else if (index_type == IndexType::FILTER) { // 加载 FilterIndex 类型的索引
            static_cast<FilterIndex*>(index)->loadIndex(scalar_storage, file_path);
        }
    }
}