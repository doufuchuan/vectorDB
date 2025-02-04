#include "hnswlib_index.h"
#include "logger.h"
#include "constants.h"
#include <iostream>
#include <vector>
#include <fstream> // 包含 <fstream> 以使用 std::ifstream

HNSWLibIndex::HNSWLibIndex(int dim, int num_data, IndexFactory::MetricType metric, int M, int ef_construction) {
    hnswlib::SpaceInterface<float>* space;
    if (metric == IndexFactory::MetricType::L2) {
        space = new hnswlib::L2Space(dim);
    } else {
        space = new hnswlib::InnerProductSpace(dim);
    }
    this->space = space; // 初始化 space 成员变量
    index = new hnswlib::HierarchicalNSW<float>(space, num_data, M, ef_construction);
}

void HNSWLibIndex::insert_vectors(const std::vector<float>& data, uint64_t label) {
    index->addPoint(data.data(), static_cast<hnswlib::labeltype>(label));
}

std::pair<std::vector<long>, std::vector<float>> HNSWLibIndex::search_vectors(const std::vector<float>& query, int k, const roaring_bitmap_t* bitmap, int ef_search) {
    index->setEf(ef_search);

    RoaringBitmapIDFilter* selector = nullptr;
    if (bitmap != nullptr) {
        selector = new RoaringBitmapIDFilter(bitmap);
    } 

    auto result = index->searchKnn(query.data(), k, selector);

    std::vector<long> indices;
    std::vector<float> distances;
    while (!result.empty()) { // 检查result是否为空
        auto item = result.top();
        indices.push_back(item.second);
        distances.push_back(item.first);
        result.pop();
    }

    if (bitmap != nullptr) {
         delete selector;
    } 

    return {indices, distances};
}

void HNSWLibIndex::saveIndex(const std::string& file_path) { // 添加 saveIndex 方法实现
    index->saveIndex(file_path);
}

void HNSWLibIndex::loadIndex(const std::string& file_path) { // 添加 loadIndex 方法实现
    std::ifstream file(file_path); // 尝试打开文件
    if (file.good()) { // 检查文件是否存在
        file.close();
        index->loadIndex(file_path, space, max_elements);
    } else {
        GlobalLogger->warn("File not found: {}. Skipping loading index.", file_path);
    }
}