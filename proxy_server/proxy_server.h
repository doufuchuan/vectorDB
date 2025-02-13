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
};
