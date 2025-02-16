#include "httplib.h"
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <vector>
#include <mutex>

// 节点信息结构
struct NodeInfo {
    std::string nodeId;
    std::string url;
    int role; // 例如，0 表示主节点，1 表示从节点
};

struct NodePartitionInfo {
    int partitionId;
    std::vector<NodeInfo> nodes; // 存储具有相同 partitionId 的所有节点
};

struct NodePartitionConfig {
    std::string partitionKey_; // 分区键
    int numberOfPartitions_;   // 分区的数量
    std::map<int, NodePartitionInfo> nodesInfo;
};

class ProxyServer {
public:
    ProxyServer(const std::string& masterServerHost, int masterServerPort, const std::string& instanceId);
    ~ProxyServer();
    void start(int port);

private:
    std::string masterServerHost_; // Master Server 的主机地址
    int masterServerPort_;         // Master Server 的端口
    std::string instanceId_;       // 当前 Proxy Server 所属的实例 ID
    CURL* curlHandle_;
    httplib::Server httpServer_;
    std::vector<NodeInfo> nodes_[2]; // 使用两个数组
    std::atomic<int> activeNodesIndex_; // 指示当前活动的数组索引
    std::atomic<size_t> nextNodeIndex_; // 轮询索引
    std::mutex nodesMutex_; // 保证节点信息的线程安全访问
    bool running_; // 控制定时器线程的运行

    NodePartitionConfig nodePartitions_[2]; // 使用两个数组进行无锁交替更新
    std::atomic<int> activePartitionIndex_; // 指示当前活动的分区数组索引

    std::set<std::string> readPaths_;  // 读请求的路径集合
    std::set<std::string> writePaths_; // 写请求的路径集合

    void setupForwarding();
    void forwardRequest(const httplib::Request& req, httplib::Response& res, const std::string& path);
    void handleTopologyRequest(httplib::Response& res);
    void initCurl();
    void cleanupCurl();
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp);
    void fetchAndUpdateNodes(); // 获取并更新节点信息
    void startNodeUpdateTimer(); // 启动节点更新定时器
    bool extractPartitionKeyValue(const httplib::Request& req, std::string& partitionKeyValue);
    int calculatePartitionId(const std::string& partitionKeyValue);
    bool selectTargetNode(const httplib::Request& req, int partitionId, const std::string& path, NodeInfo& targetNode);
    void forwardToTargetNode(const httplib::Request& req, httplib::Response& res, const std::string& path, const NodeInfo& targetNode);
    void broadcastRequestToAllPartitions(const httplib::Request& req, httplib::Response& res, const std::string& path);
    httplib::Response sendRequestToPartition(const httplib::Request& originalReq, const std::string& path, int partitionId);
    void processAndRespondToBroadcast(httplib::Response& res, const std::vector<httplib::Response>& allResponses, uint k);
    // 新增的获取分区配置的方法
    void fetchAndUpdatePartitionConfig();
    
    // 新增的方法
    void startPartitionUpdateTimer();
};
