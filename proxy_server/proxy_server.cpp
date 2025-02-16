#include "proxy_server.h"
#include "logger.h"// 包含 rapidjson 头文件
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <future>
#include <functional>

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
    std::string partitionKeyValue;
    if (!extractPartitionKeyValue(req, partitionKeyValue)) {
        GlobalLogger->debug("Partition key value not found, broadcasting request to all partitions");
        broadcastRequestToAllPartitions(req, res, path);
        return;
    }

    int partitionId = calculatePartitionId(partitionKeyValue);

    NodeInfo targetNode;
    if (!selectTargetNode(req, partitionId, path, targetNode)) {
        res.status = 503;
        res.set_content("No suitable node found for forwarding", "text/plain");
        return;
    }

    forwardToTargetNode(req, res, path, targetNode);
}

void ProxyServer::broadcastRequestToAllPartitions(const httplib::Request& req, httplib::Response& res, const std::string& path) {
    // 解析请求以获取 k 的值
    rapidjson::Document doc;
    doc.Parse(req.body.c_str());
    if (doc.HasParseError() || !doc.HasMember("k") || !doc["k"].IsInt()) {
        res.status = 400;
        res.set_content("Invalid request: missing or invalid 'k'", "text/plain");
        return;
    }

    int k = doc["k"].GetInt();

    int activePartitionIndex = activePartitionIndex_.load();
    const auto& partitionConfig = nodePartitions_[activePartitionIndex];
    std::vector<std::future<httplib::Response>> futures;

    std::unordered_set<int> sentPartitionIds;

    for (const auto& partition : partitionConfig.nodesInfo) {
        int partitionId = partition.first; 
        if (sentPartitionIds.find(partitionId) != sentPartitionIds.end()) {
            // 如果已经发送过请求，跳过
            continue;
        }

        futures.push_back(std::async(std::launch::async, &ProxyServer::sendRequestToPartition, this, req, path, partition.first));
        // 发送请求后，将分区ID添加到已发送的集合中
        sentPartitionIds.insert(partitionId);
    }

    // 收集和处理响应
    std::vector<httplib::Response> allResponses;
    for (auto& future : futures) {
        allResponses.push_back(future.get());
    }

    // 处理响应，包括排序和提取最多 K 个结果
    processAndRespondToBroadcast(res, allResponses, k);
}

httplib::Response ProxyServer::sendRequestToPartition(const httplib::Request& originalReq, const std::string& path, int partitionId) {
    NodeInfo targetNode;
    if (!selectTargetNode(originalReq, partitionId, path, targetNode)) {
        GlobalLogger->error("Failed to select target node for partition ID: {}", partitionId);
        // 创建一个空的响应对象并返回
        return httplib::Response();
    }

    // 构建目标 URL
    std::string targetUrl = targetNode.url + path;
    GlobalLogger->info("Forwarding request to partition node: {}", targetUrl);

    // 使用 CURL 发送请求
    CURL *curl = curl_easy_init();
    if (!curl) {
        GlobalLogger->error("CURL initialization failed");
        return httplib::Response();
    }

    curl_easy_setopt(curl, CURLOPT_URL, targetUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    if (originalReq.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, originalReq.body.c_str());
    } else {
        // 对于其他类型的请求，添加相应的处理逻辑
        // 例如 GET 请求：
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        GlobalLogger->error("Curl request failed: {}", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return httplib::Response();
    }

    // 创建响应对象并填充数据
    httplib::Response httpResponse;
    httpResponse.status = 200; // 根据实际情况设置状态码
    httpResponse.set_content(response_data, "application/json");

    curl_easy_cleanup(curl);
    return httpResponse;
}


void ProxyServer::processAndRespondToBroadcast(httplib::Response& res, const std::vector<httplib::Response>& allResponses, uint k) {
    GlobalLogger->debug("Processing broadcast responses. Expected max results: {}", k);

    struct CombinedResult {
        double distance;
        double vector;
    };

    std::vector<CombinedResult> allResults;

    // 解析并合并响应
    for (const auto& response : allResponses) {
        GlobalLogger->debug("Processing response from a partition");
        if (response.status == 200) {
            GlobalLogger->debug("Response parsed successfully");
            rapidjson::Document doc;
            doc.Parse(response.body.c_str());
            
            if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("vectors") && doc.HasMember("distances")) {
                const auto& vectors = doc["vectors"].GetArray();
                const auto& distances = doc["distances"].GetArray();

                for (rapidjson::SizeType i = 0; i < vectors.Size(); ++i) {
                    CombinedResult result = {distances[i].GetDouble(), vectors[i].GetDouble()};
                    allResults.push_back(result);
                }
            }
        } else {
            GlobalLogger->debug("Response status: {}", response.status);
        }
    }

    // 对 allResults 根据 distances 排序
    GlobalLogger->debug("Sorting combined results");
    std::sort(allResults.begin(), allResults.end(), [](const CombinedResult& a, const CombinedResult& b) {
        return a.distance < b.distance; // 以 distance 作为排序的关键字
    });

    // 提取最多 K 个结果
    GlobalLogger->debug("Resizing results to max of {} from {}", k, allResults.size());
    if (allResults.size() > k) {
        allResults.resize(k);
    }

    // 构建最终响应
    GlobalLogger->debug("Building final response");
    rapidjson::Document finalDoc;
    finalDoc.SetObject();
    rapidjson::Document::AllocatorType& allocator = finalDoc.GetAllocator();

    rapidjson::Value finalVectors(rapidjson::kArrayType);
    rapidjson::Value finalDistances(rapidjson::kArrayType);

    for (const auto& result : allResults) {
        finalVectors.PushBack(result.vector, allocator);
        finalDistances.PushBack(result.distance, allocator);
    }

    finalDoc.AddMember("vectors", finalVectors, allocator);
    finalDoc.AddMember("distances", finalDistances, allocator);
    finalDoc.AddMember("retCode", 0, allocator);

    GlobalLogger->debug("Final response prepared");

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    finalDoc.Accept(writer);

    res.set_content(buffer.GetString(), "application/json");
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
        if (nodeVal["status"].GetInt() == 1) { // 只添加状态为 1 的节点
            NodeInfo node;
            node.nodeId = nodeVal["nodeId"].GetString();
            node.url = nodeVal["url"].GetString();
            node.role = nodeVal["role"].GetInt();
            nodes_[inactiveIndex].push_back(node);
        } else {
            GlobalLogger->info("Skipping inactive node: {}", nodeVal["nodeId"].GetString());
        }
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

    // 添加分区配置信息
    int activePartitionIndex = activePartitionIndex_.load();
    const auto& partitionConfig = nodePartitions_[activePartitionIndex];
    
    rapidjson::Value partitionConfigObj(rapidjson::kObjectType);
    partitionConfigObj.AddMember("partitionKey", rapidjson::Value(partitionConfig.partitionKey_.c_str(), allocator), allocator);
    partitionConfigObj.AddMember("numberOfPartitions", partitionConfig.numberOfPartitions_, allocator);

    // 添加分区节点信息
    rapidjson::Value partitionNodes(rapidjson::kArrayType);
    for (const auto& partition : partitionConfig.nodesInfo) {
        rapidjson::Value partitionInfo(rapidjson::kObjectType);
        partitionInfo.AddMember("partitionId", partition.first, allocator);

        rapidjson::Value nodesInPartition(rapidjson::kArrayType);
        for (const auto& node : partition.second.nodes) {
            rapidjson::Value nodeObj(rapidjson::kObjectType);
            nodeObj.AddMember("nodeId", rapidjson::Value(node.nodeId.c_str(), allocator), allocator);
            nodesInPartition.PushBack(nodeObj, allocator);
        }

        partitionInfo.AddMember("nodes", nodesInPartition, allocator);
        partitionNodes.PushBack(partitionInfo, allocator);
    }
    partitionConfigObj.AddMember("partitions", partitionNodes, allocator);
    doc.AddMember("partitionConfig", partitionConfigObj, allocator);

    // 转换 JSON 对象为字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    // 设置响应
    res.set_content(buffer.GetString(), "application/json");
}

void ProxyServer::fetchAndUpdatePartitionConfig() {
    GlobalLogger->info("Fetching Partition Config from Master Server");
    // 使用 curl 获取分区配置
    std::string url = "http://" + masterServerHost_ + ":" + std::to_string(masterServerPort_) + "/getPartitionConfig?instanceId=" + instanceId_;
    curl_easy_setopt(curlHandle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_HTTPGET, 1L);

    std::string response_data;
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEDATA, &response_data);

    CURLcode curl_res = curl_easy_perform(curlHandle_);
    if (curl_res != CURLE_OK) {
        GlobalLogger->error("curl_easy_perform() failed: {}", curl_easy_strerror(curl_res));
        return;
    }

    // 解析响应数据并更新 nodePartitions_ 数组
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

    int inactiveIndex = activePartitionIndex_.load() ^ 1; // 获取非活动数组的索引
    nodePartitions_[inactiveIndex].nodesInfo.clear();

        // 获取 partitionKey 和 numberOfPartitions
    nodePartitions_[inactiveIndex].partitionKey_ = doc["data"]["partitionKey"].GetString();
    nodePartitions_[inactiveIndex].numberOfPartitions_ = doc["data"]["numberOfPartitions"].GetInt();


    // 填充 nodePartitions_[inactiveIndex] 数据
    const auto& partitionsArray = doc["data"]["partitions"].GetArray();
    for (const auto& partitionVal : partitionsArray) {
        int partitionId = partitionVal["partitionId"].GetInt();
        std::string nodeId = partitionVal["nodeId"].GetString();

        // 查找或创建新的 NodePartitionInfo

        auto it = nodePartitions_[inactiveIndex].nodesInfo.find(partitionId);

        if (it == nodePartitions_[inactiveIndex].nodesInfo.end()) {
            // 新建 NodePartitionInfo
            NodePartitionInfo newPartition;
            newPartition.partitionId = partitionId;
            nodePartitions_[inactiveIndex].nodesInfo[partitionId]= newPartition;
            it = std::prev(nodePartitions_[inactiveIndex].nodesInfo.end());
        }

        // 添加节点信息
        NodeInfo nodeInfo;
        nodeInfo.nodeId = nodeId;
        // nodeInfo.url 和 nodeInfo.role 需要从某处获取或者设定
        it->second.nodes.push_back(nodeInfo);
    }

    // 原子地切换活动数组索引
    activePartitionIndex_.store(inactiveIndex);

    GlobalLogger->info("Partition Config updated successfully");

}

// 解析请求中的分区键
bool ProxyServer::extractPartitionKeyValue(const httplib::Request& req, std::string& partitionKeyValue) {
    GlobalLogger->debug("Extracting partition key value from request");

    rapidjson::Document doc;
    if (doc.Parse(req.body.c_str()).HasParseError()) {
        GlobalLogger->debug("Failed to parse request body as JSON");
        return false;
    }

    int activePartitionIndex = activePartitionIndex_.load();
    const auto& partitionConfig = nodePartitions_[activePartitionIndex];

    if (!doc.HasMember(partitionConfig.partitionKey_.c_str())) {
        GlobalLogger->debug("Partition key not found in request");
        return false;
    }

    const rapidjson::Value& keyVal = doc[partitionConfig.partitionKey_.c_str()];
    if (keyVal.IsString()) {
        partitionKeyValue = keyVal.GetString();
    } else if (keyVal.IsInt()) {
        partitionKeyValue = std::to_string(keyVal.GetInt());
    } else {
        GlobalLogger->debug("Unsupported type for partition key");
        return false;
    }

    GlobalLogger->debug("Partition key value extracted: {}", partitionKeyValue);
    return true;
}


// 根据分区键值计算分区 ID
int ProxyServer::calculatePartitionId(const std::string& partitionKeyValue) {
    GlobalLogger->debug("Calculating partition ID for key value: {}", partitionKeyValue);

    int activePartitionIndex = activePartitionIndex_.load();
    const auto& partitionConfig = nodePartitions_[activePartitionIndex];

    // 使用哈希函数处理 partitionKeyValue
    std::hash<std::string> hasher;
    size_t hashValue = hasher(partitionKeyValue);

    // 使用哈希值计算分区 ID
    int partitionId = static_cast<int>(hashValue % partitionConfig.numberOfPartitions_);
    GlobalLogger->debug("Calculated partition ID: {}", partitionId);

    return partitionId;
}



bool ProxyServer::selectTargetNode(const httplib::Request& req, int partitionId, const std::string& path, NodeInfo& targetNode) {
    GlobalLogger->debug("Selecting target node for partition ID: {}", partitionId);

    bool forceMaster = req.has_param("forceMaster") && req.get_param_value("forceMaster") == "true";
    int activeNodeIndex = activeNodesIndex_.load();
    const auto& partitionConfig = nodePartitions_[activePartitionIndex_.load()];
    const auto& partitionNodes = partitionConfig.nodesInfo.find(partitionId);
    
    if (partitionNodes == partitionConfig.nodesInfo.end()) {
        GlobalLogger->error("No nodes found for partition ID: {}", partitionId);
        return false;
    }

    // 获取所有可用的节点
    std::vector<NodeInfo> availableNodes;
    for (const auto& partitionNode : partitionNodes->second.nodes) {
        auto it = std::find_if(nodes_[activeNodeIndex].begin(), nodes_[activeNodeIndex].end(),
                               [&partitionNode](const NodeInfo& n) { return n.nodeId == partitionNode.nodeId; });
        if (it != nodes_[activeNodeIndex].end()) {
            availableNodes.push_back(*it);
        }
    }

    if (availableNodes.empty()) {
        GlobalLogger->error("No available nodes for partition ID: {}", partitionId);
        return false;
    }

    if (forceMaster || writePaths_.find(path) != writePaths_.end()) {
        // 寻找主节点
        for (const auto& node : availableNodes) {
            if (node.role == 0) {
                targetNode = node;
                return true;
            }
        }
        GlobalLogger->error("No master node available for partition ID: {}", partitionId);
        return false;
    } else {
        // 读请求 - 使用轮询算法选择节点
        size_t nodeIndex = nextNodeIndex_.fetch_add(1) % availableNodes.size();
        targetNode = availableNodes[nodeIndex];
        return true;
    }
}


// 构建 URL 并使用 CURL 转发请求
void ProxyServer::forwardToTargetNode(const httplib::Request& req, httplib::Response& res, const std::string& path, const NodeInfo& targetNode) {
    GlobalLogger->debug("Forwarding request to target node: {}", targetNode.nodeId);
    // 构建目标 URL
    std::string targetUrl = targetNode.url + path;
    GlobalLogger->info("Forwarding request to: {}", targetUrl);

    // 设置 CURL 选项
    curl_easy_setopt(curlHandle_, CURLOPT_URL, targetUrl.c_str());
    if (req.method == "POST") {
        curl_easy_setopt(curlHandle_, CURLOPT_POSTFIELDS, req.body.c_str());
    } else {
        // 对于除 POST 外的其他方法，您可能需要调整此处
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
void ProxyServer::startNodeUpdateTimer() {
    std::thread([this]() { // 创建一个新线程，捕获当前对象的this指针
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); //线程休眠
            fetchAndUpdateNodes(); //唤醒后，调用函数来获取和更新节点信息
        }
    }).detach(); // 分离线程，允许它独立于主程序进行
}
