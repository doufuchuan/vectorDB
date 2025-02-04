#include "scalar_storage.h"
#include "logger.h"
#include <rocksdb/db.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h> // 包含rapidjson/stringbuffer.h头文件
#include <rapidjson/writer.h>
#include <vector>

ScalarStorage::ScalarStorage(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    if (!status.ok()) {
        GlobalLogger->error("Failed to open RocksDB: {}", status.ToString());
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }
}

ScalarStorage::~ScalarStorage() {
    delete db_;
}

void ScalarStorage::insert_scalar(uint64_t id, const rapidjson::Document& data) { // 将参数类型更改为rapidjson::Document
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    std::string value = buffer.GetString();

    rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), std::to_string(id), value);
    if (!status.ok()) {
        GlobalLogger->error("Failed to insert scalar: {}", status.ToString()); // 使用GlobalLogger打印错误日志
    }
}

rapidjson::Document ScalarStorage::get_scalar(uint64_t id) { // 将返回类型更改为rapidjson::Document
    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), std::to_string(id), &value);
    if (!status.ok()) {
        return rapidjson::Document(); // 返回一个空的rapidjson::Document对象
    }

    rapidjson::Document data;
    data.Parse(value.c_str());

    // 打印从ScalarStorage获取的数据和rocksdb::Status status
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    GlobalLogger->debug("Data retrieved from ScalarStorage: {}, RocksDB status: {}", buffer.GetString(), status.ToString()); // 添加rocksdb::Status status

    return data;
}

void ScalarStorage::put(const std::string& key, const std::string& value) {
    rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), key, value);
    if (!status.ok()) {
        GlobalLogger->error("Failed to put key-value pair: {}", status.ToString());
    }
}

std::string ScalarStorage::get(const std::string& key) {
    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok()) {
        //GlobalLogger->error("Failed to get value for key {}: {}", key, status.ToString());
        return "";
    }
    return value;
}