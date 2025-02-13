#include "proxy_server.h"
#include "logger.h"// 包含 rapidjson 头文件
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <iostream>
#include <sstream>

ProxyServer::ProxyServer(const std::string& masterServerHost, int masterServerPort, const std::string& instanceId)
: masterServerHost_(masterServerHost), masterServerPort_(masterServerPort), instanceId_(instanceId), curlHandle_(nullptr), activeNodesIndex_(0) , nextNodeIndex_(0), running_(true) {
    initCurl();
    setupForwarding();
    startNodeUpdateTimer(); // 启动节点更新定时器

    // 定义读请求路径
    readPaths_ = {"/search"};

    // 定义写请求路径
    writePaths_ = {"/upsert"};
}


ProxyServer::~ProxyServer() {
    running_ = false; // 停止定时器循环
    cleanupCurl();
}

void ProxyServer::initCurl() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curlHandle_ = curl_easy_init();
    if (curlHandle_) {
        curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPINTVL, 60L);
    }
}

void ProxyServer::cleanupCurl() {
    if (curlHandle_) {
        curl_easy_cleanup(curlHandle_);
    }
    curl_global_cleanup();
}

void ProxyServer::start(int port) {
    GlobalLogger->info("Proxy server starting on port {}", port);
    fetchAndUpdateNodes(); // 获取节点信息
    httpServer_.listen("0.0.0.0", port);
}

void ProxyServer::setupForwarding() {
    // 对 /search 路径的POST请求进行转发
    httpServer_.Post("/search", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /search");
        forwardRequest(req, res, "/search");
    });

    // 对 /insert 路径的POST请求进行转发
    httpServer_.Post("/insert", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /insert");
        forwardRequest(req, res, "/insert");
    });

    // 对 /upsert 路径的POST请求进行转发
    httpServer_.Post("/upsert", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /upsert");
        forwardRequest(req, res, "/upsert");
    });

    // 对 /query 路径的POST请求进行转发
    httpServer_.Post("/query", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /query");
        forwardRequest(req, res, "/query");
    });

    // 对 /admin/snapshot 路径的POST请求进行转发
    httpServer_.Post("/admin/snapshot", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /admin/snapshot");
        forwardRequest(req, res, "/admin/snapshot");
    });

    // 对 /admin/setLeader 路径的POST请求进行转发
    httpServer_.Post("/admin/setLeader", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /admin/setLeader");
        forwardRequest(req, res, "/admin/setLeader");
    });

    // 对 /admin/addFollower 路径的POST请求进行转发
    httpServer_.Post("/admin/addFollower", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding POST /admin/addFollower");
        forwardRequest(req, res, "/admin/addFollower");
    });

    // 对 /admin/listNode 路径的GET请求进行转发
    httpServer_.Get("/admin/listNode", [this](const httplib::Request& req, httplib::Response& res) {
        GlobalLogger->info("Forwarding GET /admin/listNode");
        forwardRequest(req, res, "/admin/listNode");
    });

    // 添加新路由以返回拓扑信息
    httpServer_.Get("/topology", [this](const httplib::Request&, httplib::Response& res) {
        this->handleTopologyRequest(res);
    });

    // 在此处根据需要添加更多的转发规则
    
}

/**
 * 将接收到的http请求转发到后端节点
 * 
 * 基于请求级别的读写分离
 */
void ProxyServer::forwardRequest(const httplib::Request& req, httplib::Response& res, const std::string& path) {
    int activeIndex = activeNodesIndex_.load(); // 读取当前活动的数组索引
    if (nodes_[activeIndex].empty()) {
        GlobalLogger->error("No available nodes for forwarding");
        res.status = 503;
        res.set_content("Service Unavailable", "text/plain");
        return;
    }

    size_t nodeIndex = 0;
    // 检查是否需要强制路由到主节点
    bool forceMaster = (req.has_param("forceMaster") && req.get_param_value("forceMaster") == "true");
    if (forceMaster || writePaths_.find(path) != writePaths_.end()) {
        // 强制主节点或写请求 - 寻找 role 为 0 的节点
        for (size_t i = 0; i < nodes_[activeIndex].size(); ++i) {
            if (nodes_[activeIndex][i].role == 0) {
                nodeIndex = i;
                break;
            }
        }
    } else {    
        // 读请求 - 轮询选择任何角色的节点
        nodeIndex = nextNodeIndex_.fetch_add(1) % nodes_[activeIndex].size();
    }

    const auto& targetNode = nodes_[activeIndex][nodeIndex];
    std::string targetUrl = targetNode.url + path;
    GlobalLogger->info("Forwarding request to: {}", targetUrl);

    // 设置 CURL 选项
    curl_easy_setopt(curlHandle_, CURLOPT_URL, targetUrl.c_str());
    if (req.method == "POST") {
        curl_easy_setopt(curlHandle_, CURLOPT_POSTFIELDS, req.body.c_str());
    } else {
        curl_easy_setopt(curlHandle_, CURLOPT_HTTPGET, 1L);
    }
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEFUNCTION, writeCallback);

    // 响应数据容器
    std::string response_data;
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEDATA, &response_data);

    // 执行 CURL 请求
    CURLcode curl_res = curl_easy_perform(curlHandle_);
    if (curl_res != CURLE_OK) {
        GlobalLogger->error("curl_easy_perform() failed: {}", curl_easy_strerror(curl_res));
        res.status = 500;
        res.set_content("Internal Server Error", "text/plain");
    } else {
        GlobalLogger->info("Received response from server");
        // 确保响应数据不为空
        if (response_data.empty()) {
            GlobalLogger->error("Received empty response from server");
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        } else {
            res.set_content(response_data, "application/json");
        }
    }
}

size_t ProxyServer::writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void ProxyServer::fetchAndUpdateNodes() {
    GlobalLogger->info("Fetching nodes from Master Server");

    // 构建请求 URL
    std::string url = "http://" + masterServerHost_ + ":" + std::to_string(masterServerPort_) + "/getInstance?instanceId=" + instanceId_;
    GlobalLogger->debug("Requesting URL: {}", url);

    // 设置 CURL 选项
    curl_easy_setopt(curlHandle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_HTTPGET, 1L);
    std::string response_data;
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEDATA, &response_data);

    // 执行 CURL 请求
    CURLcode curl_res = curl_easy_perform(curlHandle_);
    if (curl_res != CURLE_OK) {
        GlobalLogger->error("curl_easy_perform() failed: {}", curl_easy_strerror(curl_res));
        return;
    }

    // 解析响应数据
    rapidjson::Document doc;
    if (doc.Parse(response_data.c_str()).HasParseError()) {
        GlobalLogger->error("Failed to parse JSON response");
        return;
    }

    // 检查返回码
    if (doc["retCode"].GetInt() != 0) {
        GlobalLogger->error("Error from Master Server: {}", doc["msg"].GetString());
        return;
    }

    int inactiveIndex = activeNodesIndex_.load() ^ 1; // 获取非活动数组的索引
    nodes_[inactiveIndex].clear(); // 清空非活动数组，准备添加新的节点信息
    const auto& nodesArray = doc["data"]["nodes"].GetArray();
    for (const auto& nodeVal : nodesArray) {
        NodeInfo node;
        node.nodeId = nodeVal["nodeId"].GetString();
        node.url = nodeVal["url"].GetString();
        node.role = nodeVal["role"].GetInt();
        nodes_[inactiveIndex].push_back(node);
    }

    // 原子地切换活动数组索引
    activeNodesIndex_.store(inactiveIndex);
    GlobalLogger->info("Nodes updated successfully");
}


void ProxyServer::handleTopologyRequest(httplib::Response& res) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    // 添加 Master Server 信息
    doc.AddMember("masterServer", rapidjson::Value(masterServerHost_.c_str(), allocator), allocator);
    doc.AddMember("masterServerPort", masterServerPort_, allocator);

    // 添加 instanceId
    doc.AddMember("instanceId", rapidjson::Value(instanceId_.c_str(), allocator), allocator);

    // 添加节点信息
    rapidjson::Value nodesArray(rapidjson::kArrayType);
    int activeIndex = activeNodesIndex_.load();
    for (const auto& node : nodes_[activeIndex]) {
        rapidjson::Value nodeObj(rapidjson::kObjectType);
        nodeObj.AddMember("nodeId", rapidjson::Value(node.nodeId.c_str(), allocator), allocator);
        nodeObj.AddMember("url", rapidjson::Value(node.url.c_str(), allocator), allocator);
        nodeObj.AddMember("role", node.role, allocator);
        nodesArray.PushBack(nodeObj, allocator);
    }
    doc.AddMember("nodes", nodesArray, allocator);

    // 转换 JSON 对象为字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    // 设置响应
    res.set_content(buffer.GetString(), "application/json");
}


void ProxyServer::startNodeUpdateTimer() {
    std::thread([this]() { // 创建一个新线程，捕获当前对象的this指针
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); //线程休眠
            fetchAndUpdateNodes(); //唤醒后，调用函数来获取和更新节点信息
        }
    }).detach(); // 分离线程，允许它独立于主程序进行
}
