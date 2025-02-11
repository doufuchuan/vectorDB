#pragma once

#include <libnuraft/nuraft.hxx>
#include <atomic>
#include "vector_database.h" // 包含 VectorDatabase 类的头文件

using namespace nuraft;

class log_state_machine : public state_machine {
public:
    log_state_machine(): last_committed_idx_(0){}
    ~log_state_machine() {}
    void setVectorDatabase(VectorDatabase* vector_database); // 重命名为 setVectorDatabase
    ptr<buffer> commit(const ulong log_idx, buffer& data);

    ptr<buffer> pre_commit(const ulong log_idx, buffer& data);
    void commit_config(const ulong log_idx, ptr<cluster_config>& new_conf) {
        // Nothing to do with configuration change. Just update committed index.
        last_committed_idx_ = log_idx;
    }
    void rollback(const ulong log_idx, buffer& data){}
    int read_logical_snp_obj(snapshot& s,void*& user_snp_ctx,ulong obj_id,ptr<buffer>& data_out,bool& is_last_obj){
        return 0;
    }
    void save_logical_snp_obj(snapshot& s,ulong& obj_id,buffer& data,bool is_first_obj,bool is_last_obj){}
    bool apply_snapshot(snapshot& s) {
        return true;
    }
    void free_user_snp_ctx(void*& user_snp_ctx) {}
    ptr<snapshot> last_snapshot() {
        return nullptr;
    }
    ulong last_commit_index() {
        return last_committed_idx_;
    }
    
    void create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done){}

private:
    // Last committed Raft log number.
    std::atomic<uint64_t> last_committed_idx_;
    VectorDatabase* vector_database_; // 添加一个 VectorDatabase 指针成员变量
};