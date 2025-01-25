#include "faiss_index.h"
#include "logger.h"
#include "constants.h" 
#include <iostream>
#include <vector>

FaissIndex::FaissIndex(faiss::Index* index) : index(index) {}

/**
 * @brief 将向量数据和对应的标签写入索引
 * @param data 需要写入扁平索引的向量数据
 * @param label 与data对应的向量ID
 */
void FaissIndex::insert_vectors(const std::vector<float>& data, uint64_t label) {
    long id = static_cast<long>(label);
    //1表示写入单个向量，data.data()提供向量数据的指针，&id提供向量ID
    index->add_with_ids(1, data.data(), &id);
}

/**
 * @brief 在索引中查询与待查询向量最邻近的k个向量
 * @param query 待查询向量
 * @param k 需要查询的最邻近向量个数
 */
std::pair<std::vector<long>, std::vector<float>> FaissIndex::search_vectors(const std::vector<float>& query, int k) {
    int dim = index->d; //获取索引中向量的维度
    int num_queries = query.size() / dim; //查询向量的数量
    //存储查询结果的动态数组
    std::vector<long> indices(num_queries * k);
    //存储查询结果距离的动态数组
    std::vector<float> distances(num_queries * k);

    index->search(num_queries, query.data(), k, distances.data(), indices.data());

    GlobalLogger->debug("Retrieved values:");
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] != -1) {
            GlobalLogger->debug("ID: {}, Distance: {}", indices[i], distances[i]);
        } else {
            GlobalLogger->debug("No specific value found");
        }
    }
    return {indices, distances};
}