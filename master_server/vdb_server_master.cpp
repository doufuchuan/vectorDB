#include "master_server.h"
#include "logger.h"
#include <iostream>

int main() {
    init_global_logger();
    set_log_level(spdlog::level::debug);

    std::string etcdEndpoints = "http://127.0.0.1:2379";
    int httpPort = 6060;

    try {
        MasterServer masterServer(etcdEndpoints, httpPort);
        GlobalLogger->info("Starting MasterServer on port {}", httpPort);
        masterServer.run();
    } catch (const std::exception& e) {
        GlobalLogger->error("Exception occurred in MasterServer: {}", e.what());
        return -1;
    }

    return 0;
}
