#pragma once

#include <faiss/Index.h>
#include <faiss/utils/utils.h>
#include "faiss/impl/IDSelector.h"
#include "roaring/roaring.h"
#include <vector>

// 定义 RoaringBitmapIDSelector 结构体
struct RoaringBitmapIDSelector : faiss::IDSelector {
    RoaringBitmapIDSelector(const roaring_bitmap_t* bitmap) : bitmap_(bitmap) {}

    bool is_member(int64_t id) const final;

    ~RoaringBitmapIDSelector() override {}

    const roaring_bitmap_t* bitmap_;
};

class FaissIndex {
public:
    FaissIndex(faiss::Index* index); //接收一个指向FAISS索引对象的指针
    void insert_vectors(const std::vector<float>& data, uint64_t label); //将向量数据和对应的标签写入索引
    void remove_vectors(const std::vector<long>& ids); // 添加remove_vectors接口
    std::pair<std::vector<long>, std::vector<float>> search_vectors(const std::vector<float>& query, int k, const roaring_bitmap_t* bitmap = nullptr);

private:
    faiss::Index* index; //存储指向FAISS索引对象的指针
};