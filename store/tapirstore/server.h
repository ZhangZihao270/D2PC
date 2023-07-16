

#ifndef _TAPIR_SERVER_H_
#define _TAPIR_SERVER_H_

#include "replication/ir/replica.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/tapirstore/store.h"
#include "store/tapirstore/tapir-proto.pb.h"

namespace tapirstore {

using opid_t = replication::ir::opid_t;
using RecordEntry = replication::ir::RecordEntry;

class Server : public replication::ir::IRAppReplica
{
public:
    Server(bool linearizable);
    virtual ~Server();

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

    void Load(const string &key, const string &value, const Timestamp timestamp);

private:
    TxnStore *store;
};

} // namespace tapirstore

#endif /* _TAPIR_SERVER_H_ */
