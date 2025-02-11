#pragma once

#include <libnuraft/nuraft.hxx>

using namespace nuraft;

class log_state_machine : public state_machine {
public:
    ptr<buffer> commit(const ulong log_idx, buffer& data); //提交日志并返回结果

    void pre_commit(const ulong log_idx); //在预提交阶段执行的操作
    void rollback(const ulong log_idx); //回滚到指定日志索引处
    void snapshot_save(const ptr<snapshot>& s, const ulong log_idx);
    int32_t snapshot_load(const ptr<snapshot>& s);
    bool apply_snapshot(snapshot& s);
    ptr<snapshot> last_snapshot();
    ulong last_commit_index(); //获取最后提交的日志索引
    void create_snapshot(snapshot& s, cmd_result<bool, std::shared_ptr<std::exception>>::handler_type& h);
};
