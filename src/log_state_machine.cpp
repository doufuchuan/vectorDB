#include "log_state_machine.h"
#include <iostream>

using namespace nuraft;

ptr<buffer> log_state_machine::commit(const ulong log_idx, buffer& data) {
    std::string content(reinterpret_cast<const char*>(data.data() + data.pos()), data.size());
    std::cout << "提交日志条目: " << log_idx << ", 内容: " << content << std::endl;
    return nullptr;
}

void log_state_machine::pre_commit(const ulong log_idx) {}
void log_state_machine::rollback(const ulong log_idx) {}
void log_state_machine::snapshot_save(const ptr<snapshot>& s, const ulong log_idx) {}
int32_t log_state_machine::snapshot_load(const ptr<snapshot>& s) { return 0; }
bool log_state_machine::apply_snapshot(snapshot& s) { return true; }
ptr<snapshot> log_state_machine::last_snapshot() { return nullptr; }
ulong log_state_machine::last_commit_index() { return 0; }
void log_state_machine::create_snapshot(snapshot& s, cmd_result<bool, std::shared_ptr<std::exception>>::handler_type& h) {}
