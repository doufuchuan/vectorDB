#pragma once

#include <vector>
#include <map>
#include <string>
#include <set>
#include <memory> // 包含 <memory> 以使用 std::shared_ptr
#include "roaring/roaring.h"
#include "scalar_storage.h" // 包含 scalar_storage.h 以使用 ScalarStorage 类

class FilterIndex {
public:
    enum class Operation {
        EQUAL,
        NOT_EQUAL
    };

    FilterIndex();
    void addIntFieldFilter(const std::string& fieldname, int64_t value, uint64_t id);
    void updateIntFieldFilter(const std::string& fieldname, int64_t* old_value, int64_t new_value, uint64_t id); // 将 old_value 参数更改为指针类型
    void getIntFieldFilterBitmap(const std::string& fieldname, Operation op, int64_t value, roaring_bitmap_t* result_bitmap); // 添加 result_bitmap 参数
    std::string serializeIntFieldFilter(); // 添加 serializeIntFieldFilter 方法声明
    void deserializeIntFieldFilter(const std::string& serialized_data); // 添加 deserializeIntFieldFilter 方法声明
    void saveIndex(ScalarStorage& scalar_storage, const std::string& key); // 添加 key 参数
    void loadIndex(ScalarStorage& scalar_storage, const std::string& key); // 添加 key 参数

private:
    std::map<std::string, std::map<long, roaring_bitmap_t*>> intFieldFilter;
};