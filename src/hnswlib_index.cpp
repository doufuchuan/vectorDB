#include "hnswlib_index.h"
#include <vector>

HNSWLibIndex::HNSWLibIndex(int dim, int num_data, IndexFactory::MetricType metric, int M, int ef_construction) {
    hnswlib::SpaceInterface<float>* space;
    if (metric == IndexFactory::MetricType::L2) {
        space = new hnswlib::L2Space(dim);
    } else {
        space = new hnswlib::InnerProductSpace(dim);
    }
    index = new hnswlib::HierarchicalNSW<float>(space, num_data, M, ef_construction);
}

void HNSWLibIndex::insert_vectors(const std::vector<float>& data, uint64_t label) {
    index->addPoint(data.data(), label);
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