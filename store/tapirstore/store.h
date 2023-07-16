

#ifndef _TAPIR_STORE_H_
#define _TAPIR_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/common/backend/txnstore.h"
#include "store/common/backend/versionstore.h"

#include <set>
#include <unordered_map>

namespace tapirstore {

class Store : public TxnStore
{

public:
    Store(bool linearizable);
    ~Store();

    // Overriding from TxnStore
    void Begin(uint64_t id);
    int Get(uint64_t id, const std::string &key, std::pair<Timestamp, std::string> &value);
    int Get(uint64_t id, const std::string &key, const Timestamp &timestamp, std::pair<Timestamp, std::string> &value);
    int Prepare(uint64_t id, const Transaction &txn, const Timestamp &timestamp, Timestamp &proposed);
    void Commit(uint64_t id, uint64_t timestamp = 0);
    void Abort(uint64_t id, const Transaction &txn = Transaction());
    void Load(const std::string &key, const std::string &value, const Timestamp &timestamp);

private:
    // Are we running in linearizable (vs serializable) mode?
    bool linearizable;

    // Data store
    VersionedKVStore store;

    // TODO: comment this.
    std::unordered_map<uint64_t, std::pair<Timestamp, Transaction>> prepared;
    
    void GetPreparedWrites(std::unordered_map< std::string, std::set<Timestamp> > &writes);
    void GetPreparedReads(std::unordered_map< std::string, std::set<Timestamp> > &reads);
    void Commit(const Timestamp &timestamp, const Transaction &txn);
};

} // namespace tapirstore

#endif /* _TAPIR_STORE_H_ */
