

#ifndef _STRONG_SERVER_H_
#define _STRONG_SERVER_H_

#include "replication/lr/replica.h"
#include "store/common/truetime.h"
#include "store/strongstore/occstore.h"
#include "store/strongstore/lockstore.h"
#include "store/strongstore/strong-proto.pb.h"

using namespace replication::lr;

namespace strongstore {

class Server : public replication::lr::LRAppReplica
{
public:
    Server(Mode mode, TpcMode tpcMode, uint64_t skew, uint64_t error);
    virtual ~Server();

    void GetUpcall(const string &str1, string &str2) override;
    void PrepareUpcall(const string &str1, string &str2, bool &replicate, bool do_check) override;
    void CommitUpcall(const string &str1, string &str2) override;
    void AbortUpcall(const string &str1, string &str2) override;

    // for parallel mode to end transaction critical path early
    void PreCommitUpcall(const string &str1, string &str2) override;

    // virtual void LeaderUpcall(opnum_t opnum, const string &str1, bool &replicate, string &str2);
    // virtual void ReplicaUpcall(opnum_t opnum, const string &str1, string &str2);
    // virtual void UnloggedUpcall(const string &str1, string &str2);
    void Load(const string &key, const string &value, const Timestamp timestamp);

private:
    Mode mode;
    TpcMode tpcMode;
    TxnStore *store;
    TrueTime timeServer;
};

} // namespace strongstore

#endif /* _STRONG_SERVER_H_ */
