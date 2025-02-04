#include "faiss_index.h"
#include "logger.h"
#include "constants.h" 
#include <faiss/IndexIDMap.h>
#include <faiss/IndexFlat.h>
#include <faiss/index_io.h> // 更正头文件
#include <iostream>
#include <vector>
#include <fstream> // 包含 <fstream> 以使用 std::ifstream

// bool RoaringBitmapIDSelector::is_member(int64_t id) const {
//     return roaring_bitmap_contains(bitmap_, static_cast<uint32_t>(id));
// }

bool RoaringBitmapIDSelector::is_member(int64_t id) const{
    bool is_member = roaring_bitmap_contains(bitmap_, static_cast<uint32_t>(id)); // 获取 is_member 结果
    GlobalLogger->debug("RoaringBitmapIDSelector::is_member id: {}, is_member: {}", id, is_member); // 打印 id 和 is_member 结果

    return is_member;
}

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

void FaissIndex::remove_vectors(const std::vector<long>& ids) { // 添加remove_vectors函数实现
    faiss::IndexIDMap* id_map = dynamic_cast<faiss::IndexIDMap*>(index);
    if (id_map) {
        // 初始化IDSelectorBatch对象
        faiss::IDSelectorBatch selector(ids.size(), ids.data());
        id_map->remove_ids(selector);
    } else {
        throw std::runtime_error("Underlying Faiss index is not an IndexIDMap");
    }
}

/**
 * @brief 在索引中查询与待查询向量最邻近的k个向量
 * @param query 待查询向量
 * @param k 需要查询的最邻近向量个数
 */
std::pair<std::vector<long>, std::vector<float>> FaissIndex::search_vectors(const std::vector<float>& query, int k, const roaring_bitmap_t* bitmap) {
    int dim = index->d;
    int num_queries = query.size() / dim;
    std::vector<long> indices(num_queries * k);
    std::vector<float> distances(num_queries * k);

    // 如果传入了 bitmap 参数，则使用 RoaringBitmapIDSelector 初始化 faiss::SearchParameters 对象
    faiss::SearchParameters search_params;
    RoaringBitmapIDSelector selector(bitmap);
    if (bitmap != nullptr) {
        search_params.sel = &selector;
    }

    index->search(num_queries, query.data(), k, distances.data(), indices.data(), &search_params); // 将 search_params 传入 search 方法

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

void FaissIndex::saveIndex(const std::string& file_path) { // 添加 saveIndex 方法实现
    faiss::write_index(index, file_path.c_str());
}

void FaissIndex::loadIndex(const std::string& file_path) { // 添加 loadIndex 方法实现
    std::ifstream file(file_path); // 尝试打开文件
    if (file.good()) { // 检查文件是否存在
        file.close();
        if (index != nullptr) {
            delete index;
        }
        index = faiss::read_index(file_path.c_str());
    } else {
        GlobalLogger->warn("File not found: {}. Skipping loading index.", file_path);
    }
}