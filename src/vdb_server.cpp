#include "http_server.h"
#include "index_factory.h"
#include "vector_database.h"
#include "logger.h"

int main() {
    // 初始化全局日志记录器
    init_global_logger();
    set_log_level(spdlog::level::debug); // 设置日志级别为debug

    GlobalLogger->info("Global logger initialized");

    // 初始化全局IndexFactory实例
    int dim = 1; // 向量维度
    int num_data = 1000; // 数据量
    IndexFactory* globalIndexFactory = getGlobalIndexFactory();
    globalIndexFactory->init(IndexFactory::IndexType::FLAT, dim);
    globalIndexFactory->init(IndexFactory::IndexType::HNSW, dim, num_data);
    globalIndexFactory->init(IndexFactory::IndexType::FILTER); // 初始化 FILTER 类型索引
    GlobalLogger->info("Global IndexFactory initialized");

    // 初始化VectorDatabase对象
    std::string db_path = "ScalarStorage"; // RocksDB路径
    std::string wal_path = "WALStorage"; // RocksDB路径
    int node_id = 1; // Raft节点ID
    std::string endpoint = "127.0.0.1:8081"; // Raft节点端点
    int port = 8081; // Raft节点端口
    VectorDatabase vector_database(db_path, wal_path);
    RaftStuff raftStuff(node_id, endpoint, port);
    GlobalLogger->debug("RaftStuff object created with node_id: {}, endpoint: {}, port: {}", node_id, endpoint, port); // 添加调试日志


    vector_database.reloadDatabase();
    GlobalLogger->info("VectorDatabase initialized");

    // 创建并启动HTTP服务器
    HttpServer server("localhost", 8080, &vector_database, &raftStuff);
    GlobalLogger->info("HttpServer created");
    server.start();

    return 0;
}