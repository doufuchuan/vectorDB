#pragma once

#include "httplib.h"
#include <etcd/Client.hpp>
#include <string>
#include <list>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

enum class ServerRole { Master, Backup };

struct ServerInfo {
    std::string url;
    ServerRole role;
    rapidjson::Document toJson() const;
    static ServerInfo fromJson(const rapidjson::Document& value);
};

struct Partition {
    int partitionId;
    std::string nodeId;
};

struct PartitionConfig {
    std::string partitionKey;
    int numberOfPartitions;
    std::list<Partition> partitions; // 使用 std::list 存储分区信息
};

class MasterServer {
public:
    explicit MasterServer(const std::string& etcdEndpoints, int httpPort);
    void run();

private:
    etcd::Client etcdClient_;
    httplib::Server httpServer_;
    int httpPort_;
    std::map<std::string, int> nodeErrorCounts; // 错误计数器

    void setResponse(httplib::Response& res, int retCode, const std::string& msg, const rapidjson::Document* data = nullptr);

    void getNodeInfo(const httplib::Request& req, httplib::Response& res);
    void addNode(const httplib::Request& req, httplib::Response& res);
    void removeNode(const httplib::Request& req, httplib::Response& res);
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp);

    void getInstance(const httplib::Request& req, httplib::Response& res);
    void getPartitionConfig(const httplib::Request& req, httplib::Response& res); // 新增的函数声明
    void updatePartitionConfig(const httplib::Request& req, httplib::Response& res);

    void startNodeUpdateTimer();
    void updateNodeStates();

    void doUpdatePartitionConfig(const std::string& instanceId, const std::string& partitionKey, int numberOfPartitions, const std::list<Partition>& partitions);
    PartitionConfig doGetPartitionConfig(const std::string& instanceId);

};
