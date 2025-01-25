#pragma once

#include <vector>
#include "hnswlib/hnswlib.h"
#include "index_factory.h"

class HNSWLibIndex {
public:
    // 构造函数
    HNSWLibIndex(int dim, int num_data, IndexFactory::MetricType metric, int M = 16, int ef_construction = 200); // 将MetricType参数修改为第三个参数

    // 插入向量
    void insert_vectors(const std::vector<float>& data, uint64_t label);

    // 查询向量
    std::pair<std::vector<long>, std::vector<float>> search_vectors(const std::vector<float>& query, int k, int ef_search = 50);

private:
    int dim;
    hnswlib::SpaceInterface<float>* space;
    hnswlib::HierarchicalNSW<float>* index;
};