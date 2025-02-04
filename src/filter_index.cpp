#include "filter_index.h"
#include "logger.h" // 包含 logger.h 以使用日志记录器
#include <algorithm>
#include <set>
#include <memory>
#include <sstream>

FilterIndex::FilterIndex() {}

void FilterIndex::addIntFieldFilter(const std::string& fieldname, int64_t value, uint64_t id) {
    roaring_bitmap_t* bitmap = roaring_bitmap_create();
    roaring_bitmap_add(bitmap, id);
    intFieldFilter[fieldname][value] = bitmap;
    GlobalLogger->debug("Added int field filter: fieldname={}, value={}, id={}", fieldname, value, id); // 添加打印信息

}

void FilterIndex::updateIntFieldFilter(const std::string& fieldname, int64_t* old_value, int64_t new_value, uint64_t id) {// 将 old_value 参数更改为指针类型
    if (old_value != nullptr) {
        GlobalLogger->debug("Updated int field filter: fieldname={}, old_value={}, new_value={}, id={}", fieldname, *old_value, new_value, id);
    } else {
        GlobalLogger->debug("Updated int field filter: fieldname={}, old_value=nullptr, new_value={}, id={}", fieldname, new_value, id);
    }    
    
    auto it = intFieldFilter.find(fieldname);
    if (it != intFieldFilter.end()) {
        std::map<long, roaring_bitmap_t*>& value_map = it->second;

        // 查找旧值对应的位图，并从位图中删除 ID
        auto old_bitmap_it = (old_value != nullptr) ? value_map.find(*old_value) : value_map.end(); // 使用解引用的 old_value
        if (old_bitmap_it != value_map.end()) {
            roaring_bitmap_t* old_bitmap = old_bitmap_it->second;
            roaring_bitmap_remove(old_bitmap, id);
        }

        // 查找新值对应的位图，如果不存在则创建一个新的位图
        auto new_bitmap_it = value_map.find(new_value);
        if (new_bitmap_it == value_map.end()) {
            roaring_bitmap_t* new_bitmap = roaring_bitmap_create();
            value_map[new_value] = new_bitmap;
            new_bitmap_it = value_map.find(new_value);
        }

        roaring_bitmap_t* new_bitmap = new_bitmap_it->second;
        roaring_bitmap_add(new_bitmap, id);
    } else {
        addIntFieldFilter(fieldname, new_value, id);
    }
}

void FilterIndex::getIntFieldFilterBitmap(const std::string& fieldname, Operation op, int64_t value, roaring_bitmap_t* result_bitmap) { // 添加 result_bitmap 参数
    auto it = intFieldFilter.find(fieldname);
    if (it != intFieldFilter.end()) {
        auto& value_map = it->second;

        if (op == Operation::EQUAL) {
            auto bitmap_it = value_map.find(value);
            if (bitmap_it != value_map.end()) {
                GlobalLogger->debug("Retrieved EQUAL bitmap for fieldname={}, value={}", fieldname, value);
                roaring_bitmap_or_inplace(result_bitmap, bitmap_it->second); // 更新 result_bitmap
            }
        } else if (op == Operation::NOT_EQUAL) {
            for (const auto& entry : value_map) {
                if (entry.first != value) {
                    roaring_bitmap_or_inplace(result_bitmap, entry.second); // 更新 result_bitmap
                }
            }
            GlobalLogger->debug("Retrieved NOT_EQUAL bitmap for fieldname={}, value={}", fieldname, value);
        }
    }
}

/**
 * 将过滤索引中的位图数据序列化为字节数组，并构建一个包含所有序列化数据的字符串
 */
std::string FilterIndex::serializeIntFieldFilter() {
    std::ostringstream oss;

    for (const auto& field_entry : intFieldFilter) {
        const std::string& field_name = field_entry.first;
        const std::map<long, roaring_bitmap_t*>& value_map = field_entry.second;

        for (const auto& value_entry : value_map) {
            long value = value_entry.first;
            const roaring_bitmap_t* bitmap = value_entry.second;

            // 将位图序列化为字节数组
            uint32_t size = roaring_bitmap_portable_size_in_bytes(bitmap);
            char* serialized_bitmap = new char[size];
            roaring_bitmap_portable_serialize(bitmap, serialized_bitmap);

            // 将字段名、值和序列化的位图写入输出流
            oss << field_name << "|" << value << "|";
            oss.write(serialized_bitmap, size);
            oss << std::endl;

            delete[] serialized_bitmap;
        }
    }

    return oss.str();
}

void FilterIndex::deserializeIntFieldFilter(const std::string& serialized_data) {
    std::istringstream iss(serialized_data);

    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream line_iss(line);

        // 从输入流中读取字段名、值和序列化的位图
        std::string field_name;
        std::getline(line_iss, field_name, '|');

        std::string value_str;
        std::getline(line_iss, value_str, '|');
        long value = std::stol(value_str);

        // 读取序列化的位图
        std::string serialized_bitmap(std::istreambuf_iterator<char>(line_iss), {});

        // 反序列化位图
        roaring_bitmap_t* bitmap = roaring_bitmap_portable_deserialize(serialized_bitmap.data());

        // 将反序列化的位图插入 intFieldFilter
        intFieldFilter[field_name][value] = bitmap;
    }
}

void FilterIndex::saveIndex(ScalarStorage& scalar_storage, const std::string& key) { // 添加 key 参数
    std::string serialized_data = serializeIntFieldFilter();

    // 将序列化的数据存储到 ScalarStorage
    scalar_storage.put(key, serialized_data);
}

void FilterIndex::loadIndex(ScalarStorage& scalar_storage, const std::string& key) { // 添加 key 参数
    std::string serialized_data = scalar_storage.get(key);
    // 从序列化的数据中反序列化 intFieldFilter
    deserializeIntFieldFilter(serialized_data);
}