#include "raft_stuff.h"
#include "raft_logger_wrapper.h"

RaftStuff::RaftStuff(int node_id, const std::string& endpoint, int port, VectorDatabase* vector_database)
    : node_id(node_id), endpoint(endpoint), port_(port), vector_database_(vector_database)  {// 初始化 vector_database_ 指针
    Init();
}

void RaftStuff::Init() {
    // 初始化状态管理器和状态机
    smgr_ = cs_new<inmem_state_mgr>(node_id, endpoint, vector_database_);
    sm_ = cs_new<log_state_machine>();

    // 将 state_machine 对象强制转换为 log_state_machine 对象
    ptr<log_state_machine> log_sm = std::dynamic_pointer_cast<log_state_machine>(sm_);

    log_sm->setVectorDatabase(vector_database_); // 将 vector_database_ 参数传递给 log_state_machine 的 setVectorDatabase 函数

    // 设置ASIO服务选项
    asio_service::options asio_opt;
    asio_opt.thread_pool_size_ = 1;

    // 设置raft参数
    raft_params params;
    params.election_timeout_lower_bound_ = 100000000; // 设置为一个非常大的值
    params.election_timeout_upper_bound_ = 200000000; // 设置为一个非常大的值

    // Logger.
    std::string log_file_name = "./srv.log";
    ptr<logger_wrapper> log_wrap = cs_new<logger_wrapper>( log_file_name);

    // 初始化Raft实例
    raft_instance_ = launcher_.init(sm_, smgr_, nullptr, port_, asio_opt, params);
    GlobalLogger->debug("RaftStuff initialized with node_id: {}, endpoint: {}, port: {}", node_id, endpoint, port_); // 添加打印日志
}

/**
 * @param srv_id 新服务器ID
 * @param srv_endpoint 新服务器地址
 */
ptr< cmd_result< ptr<buffer> > > RaftStuff::addSrv(int srv_id, const std::string& srv_endpoint) {
    ptr<srv_config> peer_srv_conf = cs_new<srv_config>(srv_id, srv_endpoint);
    GlobalLogger->debug("Adding server with srv_id: {}, srv_endpoint: {}", srv_id, srv_endpoint); // 添加打印日志
    return raft_instance_->add_srv(*peer_srv_conf);
}

ptr< cmd_result< ptr<buffer> > > RaftStuff::appendEntries(const std::string& entry) {
    if (!raft_instance_ || !raft_instance_->is_leader()) {
        // 添加调试日志
        if (!raft_instance_) {
            GlobalLogger->debug("Cannot append entries: Raft instance is not available");
        } else {
            GlobalLogger->debug("Cannot append entries: Current node is not the leader");
        }
        return nullptr;
    }

    // 计算所需的内存大小
    size_t total_size = sizeof(int) + entry.size();

    // 添加调试日志
    GlobalLogger->debug("Total size of entry: {}", total_size);

    // 创建一个 Raft 日志条目
    ptr<buffer> log_entry_buffer = buffer::alloc(total_size);
    buffer_serializer bs_log(log_entry_buffer);

    bs_log.put_str(entry);

    // 添加调试日志
    GlobalLogger->debug("Created log_entry_buffer at address: {}", static_cast<const void*>(log_entry_buffer.get()));

    // 添加调试日志
    GlobalLogger->debug("Appending entry to Raft instance");

    // 将日志条目追加到 Raft 实例中
    return raft_instance_->append_entries({log_entry_buffer});
}

void RaftStuff::enableElectionTimeout(int lower_bound, int upper_bound) {
    if (raft_instance_) {
        raft_params params = raft_instance_->get_current_params();
        params.election_timeout_lower_bound_ = lower_bound;
        params.election_timeout_upper_bound_ = upper_bound;
        raft_instance_->update_params(params);
    }
}

bool RaftStuff::isLeader() const {
    if (!raft_instance_) {
        return false;
    }
    return raft_instance_->is_leader(); // 调用 raft_instance_ 的 is_leader() 方法
}

std::vector<std::tuple<int, std::string, std::string, nuraft::ulong, nuraft::ulong>> RaftStuff::getAllNodesInfo() const {
    std::vector<std::tuple<int, std::string, std::string, nuraft::ulong, nuraft::ulong>> nodes_info;

    if (!raft_instance_) {
        return nodes_info;
    }

    // 获取配置信息
    auto config = raft_instance_->get_config();
    if (!config) {
        return nodes_info;
    }

    // 获取服务器列表
    auto servers = config->get_servers();
    // 遍历所有节点并将其添加到 nodes_info 中
    for (const auto& srv : servers) {
        if (srv) {
            // 获取节点状态
            std::string node_state;
            if (srv->get_id() ==  raft_instance_->get_leader()) {
                node_state = "leader";
            } else {
                node_state = "follower";
            }
            
            // 使用正确的类型
            nuraft::raft_server::peer_info node_info = raft_instance_->get_peer_info(srv->get_id());
            nuraft::ulong last_log_idx = node_info.last_log_idx_;
            nuraft::ulong last_succ_resp_us = node_info.last_succ_resp_us_;

            nodes_info.push_back(std::make_tuple(srv->get_id(), srv->get_endpoint(), node_state, last_log_idx, last_succ_resp_us));
        }
    }

    return nodes_info;
}

std::tuple<int, std::string, std::string, nuraft::ulong, nuraft::ulong> RaftStuff::getCurrentNodesInfo() const {
    std::tuple<int, std::string, std::string, nuraft::ulong, nuraft::ulong> nodes_info;

    if (!raft_instance_) {
        return nodes_info;
    }

    // 获取配置信息
    auto config = raft_instance_->get_config();
    if (!config) {
        return nodes_info;
    }

    // 获取服务器列表
    auto servers = config->get_servers();

    for (const auto& srv : servers) {
        if (srv && srv->get_id() == node_id) {
            // 获取节点状态
            std::string node_state;
            if (srv->get_id() ==  raft_instance_->get_leader()) {
                node_state = "leader";
            } else {
                node_state = "follower";
            }
            
            // 使用正确的类型
            nuraft::raft_server::peer_info node_info = raft_instance_->get_peer_info(srv->get_id());
            nuraft::ulong last_log_idx = node_info.last_log_idx_;
            nuraft::ulong last_succ_resp_us = node_info.last_succ_resp_us_;
            nodes_info = std::make_tuple(srv->get_id(), srv->get_endpoint(), node_state, last_log_idx, last_succ_resp_us);
            break;
        }
    }

    return nodes_info;
}