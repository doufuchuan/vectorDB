#include "persistence.h"
#include "logger.h" 
#include <rapidjson/document.h> // 包含 <rapidjson/document.h> 以使用 rapidjson::Document 类型
#include <rapidjson/writer.h> // 包含 rapidjson/writer.h 以使用 Writer 类
#include <rapidjson/stringbuffer.h> // 包含 rapidjson/stringbuffer.h 以使用 StringBuffer 类
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

Persistence::Persistence() : increaseID_(1) {}

Persistence::~Persistence() {
    if (wal_log_file_.is_open()) {
        wal_log_file_.close();
    }
}

void Persistence::init(const std::string& local_path) {
    wal_log_file_.open(local_path, std::ios::in | std::ios::out | std::ios::app); // 以 std::ios::in | std::ios::out | std::ios::app 模式打开文件
    if (!wal_log_file_.is_open()) {
        GlobalLogger->error("An error occurred while writing the WAL log entry. Reason: {}", std::strerror(errno)); // 使用日志打印错误消息和原因
        throw std::runtime_error("Failed to open WAL log file at path: " + local_path);
    }
}


uint64_t Persistence::increaseID() {
    increaseID_++;
    return increaseID_;
}

uint64_t Persistence::getID() const {
    return increaseID_;
}

/**
* @param operation_type 操作类型
* @param json_data JSON 数据
* @param version 日志版本
* 首先获取一个唯一的日志 ID，然后将JSON对象数据转换为字符串格式并写入日志文件。
* 如果写入过程发生错误，则记录错误消息；
* 否则，记录写入成功的日志并强制将数据持久化到硬盘

* 预写日志格式的四个字段：
* log_id: 每次写入时自增，保证了预写日志的唯一性和有序性
* version: 日志版本，以便将来对日志格式进行变更时能做向后兼容
* operation_type: 用户实际的操作类型，例如upsert
* buffer.GetString(): 用户请求数据时完整的JSON字符串
*/
void Persistence::writeWALLog(const std::string& operation_type, const rapidjson::Document& json_data, const std::string& version) { // 添加 version 参数
    uint64_t log_id = increaseID();

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_data.Accept(writer);

    wal_log_file_ << log_id << "|" << version << "|" << operation_type << "|" << buffer.GetString() << std::endl; // 将 version 添加到日志格式中

    if (wal_log_file_.fail()) { // 检查是否发生错误
        GlobalLogger->error("An error occurred while writing the WAL log entry. Reason: {}", std::strerror(errno)); // 使用日志打印错误消息和原因
    } else {
       GlobalLogger->debug("Wrote WAL log entry: log_id={}, version={}, operation_type={}, json_data_str={}", log_id, version, operation_type, buffer.GetString()); // 打印日志
       wal_log_file_.flush(); // 强制持久化
    }
}

/**
* 一旦log_id读取成功，将系统的increaseID_更新为已写入日志ID中的最大值
*/
void Persistence::readNextWALLog(std::string* operation_type, rapidjson::Document* json_data) {
    GlobalLogger->debug("Reading next WAL log entry");

    std::string line;
    if (std::getline(wal_log_file_, line)) {
        std::istringstream iss(line);
        std::string log_id_str, version, json_data_str;

        std::getline(iss, log_id_str, '|');
        std::getline(iss, version, '|');
        std::getline(iss, *operation_type, '|'); // 使用指针参数返回 operation_type
        std::getline(iss, json_data_str, '|');

        uint64_t log_id = std::stoull(log_id_str); // 将 log_id_str 转换为 uint64_t 类型
        if (log_id > increaseID_) { // 如果 log_id 大于当前 increaseID_
            increaseID_ = log_id; // 更新 increaseID_
        }

        json_data->Parse(json_data_str.c_str()); // 使用指针参数返回 json_data

        GlobalLogger->debug("Read WAL log entry: log_id={}, operation_type={}, json_data_str={}", log_id_str, *operation_type, json_data_str);
    } else {
        wal_log_file_.clear(); //重置文件结束标志
        GlobalLogger->debug("No more WAL log entries to read");
    }
}