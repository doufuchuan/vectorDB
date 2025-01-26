#pragma once

#include <faiss/Index.h>
#include <vector>

class FaissIndex {
public:
    FaissIndex(faiss::Index* index); //接收一个指向FAISS索引对象的指针
    void insert_vectors(const std::vector<float>& data, uint64_t label); //将向量数据和对应的标签写入索引
    void remove_vectors(const std::vector<long>& ids); // 添加remove_vectors接口
    /**
    在索引中查询与待查询向量最邻近的k个向量
    返回一个包含两个动态数组的pair, 第一个动态数组是找到向量的标签，第二个动态数组是相应的距离
    */
    std::pair<std::vector<long>, std::vector<float>> search_vectors(const std::vector<float>& query, int k);

private:
    faiss::Index* index; //存储指向FAISS索引对象的指针
};