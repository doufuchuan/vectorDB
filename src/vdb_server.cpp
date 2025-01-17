#include "http_server.h"
#include "index_factory.h"
#include "logger.h"

int main() {
    // 初始化全局日志记录器
    init_global_logger();
    set_log_level(spdlog::level::debug); // 设置日志级别为debug

    GlobalLogger->info("Global logger initialized");

    // 初始化全局IndexFactory实例
    int dim = 1; // 向量维度
    IndexFactory* globalIndexFactory = getGlobalIndexFactory();
    globalIndexFactory->init(IndexFactory::IndexType::FLAT, dim);
    GlobalLogger->info("Global IndexFactory initialized");

    // 创建并启动HTTP服务器
    HttpServer server("localhost", 8080);
    GlobalLogger->info("HttpServer created");
    server.start();

    return 0;
}