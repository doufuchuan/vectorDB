#include "http_server.h"
#include "faiss_index.h"
#include "hnswlib_index.h"
#include "index_factory.h"
#include "logger.h"
#include "constants.h"
#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

/**
 * HttpServer类的构造函数，用于初始化HTTP服务器
 */
HttpServer::HttpServer(const std::string& host, int port, VectorDatabase* vector_database)
    : host(host), port(port), vector_database_(vector_database) {
    server.Post("/search", [this](const httplib::Request& req, httplib::Response& res) {
        searchHandler(req, res);
    });

    server.Post("/insert", [this](const httplib::Request& req, httplib::Response& res) {
        insertHandler(req, res);
    });

    server.Post("/upsert", [this](const httplib::Request& req, httplib::Response& res) { // 注册upsert接口
        upsertHandler(req, res);
    });

    server.Post("/query", [this](const httplib::Request& req, httplib::Response& res) { // 注册query接口
        queryHandler(req, res);
    });

}

void HttpServer::start() {
    server.listen(host.c_str(), port);
}

bool HttpServer::isRequestValid(const rapidjson::Document& json_request, CheckType check_type) {
    switch (check_type) {
        case CheckType::SEARCH:
            return json_request.HasMember(REQUEST_VECTORS) &&
                   json_request.HasMember(REQUEST_K) &&
                   (!json_request.HasMember(REQUEST_INDEX_TYPE) || json_request[REQUEST_INDEX_TYPE].IsString());
        case CheckType::INSERT:
            return json_request.HasMember(REQUEST_VECTORS) &&
                   json_request.HasMember(REQUEST_ID) &&
                   (!json_request.HasMember(REQUEST_INDEX_TYPE) || json_request[REQUEST_INDEX_TYPE].IsString());
        case CheckType::UPSERT: // 添加UPSERT逻辑
            return json_request.HasMember(REQUEST_VECTORS) &&
                   json_request.HasMember(REQUEST_ID) &&
                   (!json_request.HasMember(REQUEST_INDEX_TYPE) || json_request[REQUEST_INDEX_TYPE].IsString());
        default:
            return false;
    }
}

IndexFactory::IndexType HttpServer::getIndexTypeFromRequest(const rapidjson::Document& json_request) {
    // 获取请求参数中的索引类型
    if (json_request.HasMember(REQUEST_INDEX_TYPE)) {
        std::string index_type_str = json_request[REQUEST_INDEX_TYPE].GetString();
        if (index_type_str == INDEX_TYPE_FLAT) {
            return IndexFactory::IndexType::FLAT;
        }else if (index_type_str == INDEX_TYPE_HNSW) { // 添加对HNSW的支持
            return IndexFactory::IndexType::HNSW;
        }
    }
    return IndexFactory::IndexType::UNKNOWN; 
}

void HttpServer::searchHandler(const httplib::Request& req, httplib::Response& res) {
    GlobalLogger->debug("Received search request");

    // 解析JSON请求
    rapidjson::Document json_request;
    json_request.Parse(req.body.c_str());

    // 打印用户的输入参数
    GlobalLogger->info("Search request parameters: {}", req.body);

    // 检查JSON文档是否为有效对象
    if (!json_request.IsObject()) {
        GlobalLogger->error("Invalid JSON request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Invalid JSON request"); 
        return;
    }

    // 检查请求的合法性
    if (!isRequestValid(json_request, CheckType::SEARCH)) {
        GlobalLogger->error("Missing vectors or k parameter in the request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Missing vectors or k parameter in the request"); 
        return;
    }

    // 获取查询参数
    std::vector<float> query;
    for (const auto& q : json_request[REQUEST_VECTORS].GetArray()) {
        query.push_back(q.GetFloat());
    }
    int k = json_request[REQUEST_K].GetInt();

    GlobalLogger->debug("Query parameters: k = {}", k);

    // 获取请求参数中的索引类型
    IndexFactory::IndexType indexType = getIndexTypeFromRequest(json_request);

    // 如果索引类型为UNKNOWN，返回400错误
    if (indexType == IndexFactory::IndexType::UNKNOWN) {
        GlobalLogger->error("Invalid indexType parameter in the request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Invalid indexType parameter in the request"); 
        return;
    }

    // 使用全局IndexFactory获取索引对象
    void* index = getGlobalIndexFactory()->getIndex(indexType);

    // 根据索引类型初始化索引对象并调用search_vectors函数
    std::pair<std::vector<long>, std::vector<float>> results; // 直接声明results变量
    switch (indexType) {
        case IndexFactory::IndexType::FLAT: {
            FaissIndex* faissIndex = static_cast<FaissIndex*>(index);
            results = faissIndex->search_vectors(query, k);
            break;
        }
        case IndexFactory::IndexType::HNSW: {
            HNSWLibIndex* hnswIndex = static_cast<HNSWLibIndex*>(index);
            results = hnswIndex->search_vectors(query, k);
            break;
        }
        // 在此处添加其他索引类型的处理逻辑
        default:
            break;
    }

    // 将结果转换为JSON
    rapidjson::Document json_response;
    json_response.SetObject();
    rapidjson::Document::AllocatorType& allocator = json_response.GetAllocator();

    // 检查是否有有效的搜索结果
    bool valid_results = false;
    rapidjson::Value vectors(rapidjson::kArrayType);
    rapidjson::Value distances(rapidjson::kArrayType);
    for (size_t i = 0; i < results.first.size(); ++i) {
        if (results.first[i] != -1) {
            valid_results = true;
            vectors.PushBack(results.first[i], allocator);
            distances.PushBack(results.second[i], allocator);
        }
    }

    if (valid_results) {
        json_response.AddMember(RESPONSE_VECTORS, vectors, allocator);
        json_response.AddMember(RESPONSE_DISTANCES, distances, allocator);
    }

    // 设置响应
    json_response.AddMember(RESPONSE_RETCODE, RESPONSE_RETCODE_SUCCESS, allocator); 
    setJsonResponse(json_response, res);
}

void HttpServer::insertHandler(const httplib::Request& req, httplib::Response& res) {
    GlobalLogger->debug("Received insert request");

    // 解析JSON请求
    rapidjson::Document json_request;
    json_request.Parse(req.body.c_str());

    // 打印用户的输入参数
    GlobalLogger->info("Insert request parameters: {}", req.body);

    // 检查JSON文档是否为有效对象
    if (!json_request.IsObject()) {
        GlobalLogger->error("Invalid JSON request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Invalid JSON request");
        return;
    }

    // 检查请求的合法性
    if (!isRequestValid(json_request, CheckType::INSERT)) { // 添加对isRequestValid的调用
        GlobalLogger->error("Missing vectors or id parameter in the request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Missing vectors or k parameter in the request");
        return;
    }

    // 获取插入参数
    std::vector<float> data;
    for (const auto& d : json_request[REQUEST_VECTORS].GetArray()) {
        data.push_back(d.GetFloat());
    }
    uint64_t label = json_request[REQUEST_ID].GetUint64(); // 使用宏定义

    GlobalLogger->debug("Insert parameters: label = {}", label);

    // 获取请求参数中的索引类型
    IndexFactory::IndexType indexType = getIndexTypeFromRequest(json_request);

    // 如果索引类型为UNKNOWN，返回400错误
    if (indexType == IndexFactory::IndexType::UNKNOWN) {
        GlobalLogger->error("Invalid indexType parameter in the request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Invalid indexType parameter in the request"); 
        return;
    }

    // 使用全局IndexFactory获取索引对象
    void* index = getGlobalIndexFactory()->getIndex(indexType);

    // 根据索引类型初始化索引对象并调用insert_vectors函数
    switch (indexType) {
        case IndexFactory::IndexType::FLAT: {
            FaissIndex* faissIndex = static_cast<FaissIndex*>(index);
            faissIndex->insert_vectors(data, label);
            break;
        }
        case IndexFactory::IndexType::HNSW: { // 添加HNSW索引类型的处理逻辑
            HNSWLibIndex* hnswIndex = static_cast<HNSWLibIndex*>(index);
            hnswIndex->insert_vectors(data, label);
            break;
        }
        // 在此处添加其他索引类型的处理逻辑
        default:
            break;
    }

    // 设置响应
    rapidjson::Document json_response;
    json_response.SetObject();
    rapidjson::Document::AllocatorType& allocator = json_response.GetAllocator();

    // 添加retCode到响应
    json_response.AddMember(RESPONSE_RETCODE, RESPONSE_RETCODE_SUCCESS, allocator);

    setJsonResponse(json_response, res);
}

void HttpServer::upsertHandler(const httplib::Request& req, httplib::Response& res) {
    GlobalLogger->debug("Received upsert request");

    // 解析JSON请求
    rapidjson::Document json_request;
    json_request.Parse(req.body.c_str());

    // 检查JSON文档是否为有效对象
    if (!json_request.IsObject()) {
        GlobalLogger->error("Invalid JSON request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Invalid JSON request");
        return;
    }

    // 检查请求的合法性
    if (!isRequestValid(json_request, CheckType::UPSERT)) {
        GlobalLogger->error("Missing vectors or id parameter in the request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Missing vectors or id parameter in the request");
        return;
    }

    uint64_t label = json_request[REQUEST_ID].GetUint64();

    // 获取请求参数中的索引类型
    IndexFactory::IndexType indexType = getIndexTypeFromRequest(json_request);
    vector_database_->upsert(label, json_request, indexType);

    rapidjson::Document json_response;
    json_response.SetObject();
    rapidjson::Document::AllocatorType& response_allocator = json_response.GetAllocator();

    // 添加retCode到响应
    json_response.AddMember(RESPONSE_RETCODE, RESPONSE_RETCODE_SUCCESS, response_allocator);

    setJsonResponse(json_response, res);
}

void HttpServer::queryHandler(const httplib::Request& req, httplib::Response& res) {
    GlobalLogger->debug("Received query request");

    // 解析JSON请求
    rapidjson::Document json_request;
    json_request.Parse(req.body.c_str());

    // 检查JSON文档是否为有效对象
    if (!json_request.IsObject()) {
        GlobalLogger->error("Invalid JSON request");
        res.status = 400;
        setErrorJsonResponse(res, RESPONSE_RETCODE_ERROR, "Invalid JSON request");
        return;
    }

    // 从JSON请求中获取ID
    uint64_t id = json_request[REQUEST_ID].GetUint64(); // 使用宏REQUEST_ID

    // 查询JSON数据
    rapidjson::Document json_data = vector_database_->query(id);

    // 将结果转换为JSON
    rapidjson::Document json_response;
    json_response.SetObject();
    rapidjson::Document::AllocatorType& allocator = json_response.GetAllocator();

    // 如果查询到向量，则将json_data对象的内容合并到json_response对象中
    if (!json_data.IsNull()) {
        for (auto it = json_data.MemberBegin(); it != json_data.MemberEnd(); ++it) {
            json_response.AddMember(it->name, it->value, allocator);
        }
    }

    // 设置响应
    json_response.AddMember(RESPONSE_RETCODE, RESPONSE_RETCODE_SUCCESS, allocator);
    setJsonResponse(json_response, res);

}

void HttpServer::setJsonResponse(const rapidjson::Document& json_response, httplib::Response& res) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_response.Accept(writer);
    res.set_content(buffer.GetString(), RESPONSE_CONTENT_TYPE_JSON); // 使用宏定义
}

void HttpServer::setErrorJsonResponse(httplib::Response& res, int error_code, const std::string& errorMsg) {
    rapidjson::Document json_response;
    json_response.SetObject();
    rapidjson::Document::AllocatorType& allocator = json_response.GetAllocator();
    json_response.AddMember(RESPONSE_RETCODE, error_code, allocator);
    json_response.AddMember(RESPONSE_ERROR_MSG, rapidjson::StringRef(errorMsg.c_str()), allocator); // 使用宏定义
    setJsonResponse(json_response, res);
}