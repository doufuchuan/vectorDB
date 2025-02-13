#include "proxy_server.h"
#include "logger.h"

int main() {
    init_global_logger();
    set_log_level(spdlog::level::debug);

    std::string master_host = "127.0.0.1"; // Master Server 地址
    int master_port = 6060;                // Master Server 端口
    std::string instance_id = "instance1"; // 代理服务器所属的实例 ID
    int proxy_port = 80;                   // 代理服务器监听端口

    GlobalLogger->info("Starting ProxyServer...");
    ProxyServer proxy(master_host, master_port, instance_id);
    proxy.start(proxy_port);

    return 0;
}
