

#ifndef _IR_LOCK_SERVER_H_
#define _IR_LOCK_SERVER_H_

#include <string>
#include <unordered_map>

#include "lib/transport.h"
#include "replication/ir/replica.h"
#include "lockserver/locks-proto.pb.h"

namespace lockserver {

using opid_t = replication::ir::opid_t;
using RecordEntry = replication::ir::RecordEntry;

class LockServer : public replication::ir::IRAppReplica
{
public:
    LockServer();
    ~LockServer();

    // Invoke inconsistent operation, no return value
    void ExecInconsistentUpcall(const string &str1) override;

    // Invoke consensus operation
    void ExecConsensusUpcall(const string &str1, string &str2) override;

    // Invoke unreplicated operation
    void UnloggedUpcall(const string &str1, string &str2) override;

    // Sync
    void Sync(const std::map<opid_t, RecordEntry>& record) override;

    // Merge
    std::map<opid_t, std::string> Merge(
        const std::map<opid_t, std::vector<RecordEntry>> &d,
        const std::map<opid_t, std::vector<RecordEntry>> &u,
        const std::map<opid_t, std::string> &majority_results_in_d) override;

private:
    std::unordered_map<std::string, uint64_t> locks;
};

} // namespace lockserver

#endif /* _IR_LOCK_SERVER_H_ */
