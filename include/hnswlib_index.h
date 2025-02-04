#pragma once

#include <vector>
#include "hnswlib/hnswlib.h"
#include "roaring/roaring.h" 
#include "index_factory.h"

class HNSWLibIndex {
public:
    // 构造函数
    HNSWLibIndex(int dim, int num_data, IndexFactory::MetricType metric, int M = 16, int ef_construction = 200); // 将MetricType参数修改为第三个参数

    // 插入向量
    void insert_vectors(const std::vector<float>& data, uint64_t label);
    void saveIndex(const std::string& file_path); // 添加 saveIndex 方法声明
    void loadIndex(const std::string& file_path); // 添加 loadIndex 方法声明

    // 查询向量, 引入filter bitmap
    std::pair<std::vector<long>, std::vector<float>> search_vectors(const std::vector<float>& query, int k, const roaring_bitmap_t* bitmap = nullptr, int ef_search = 50);
    class RoaringBitmapIDFilter : public hnswlib::BaseFilterFunctor {
    public:
        RoaringBitmapIDFilter(const roaring_bitmap_t* bitmap) : bitmap_(bitmap) {}

        bool operator()(hnswlib::labeltype label) {
            return roaring_bitmap_contains(bitmap_, static_cast<uint32_t>(label));
        }

    private:
        const roaring_bitmap_t* bitmap_;
    };
private:
    // int dim;
    hnswlib::SpaceInterface<float>* space;
    hnswlib::HierarchicalNSW<float>* index;
    size_t max_elements; // 添加 max_elements 成员变量
};