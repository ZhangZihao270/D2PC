

#ifndef _STRONG_LOCK_STORE_H_
#define _STRONG_LOCK_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/transaction.h"
#include "store/common/backend/versionstore.h"
#include "store/common/backend/txnstore.h"
#include "store/common/backend/lockserver.h"

#include <map>

namespace strongstore {

class LockStore : public TxnStore
{
public:
    LockStore();
    ~LockStore();

    // Overriding from TxnStore.
    int Get(uint64_t id, const std::string &key,
        std::pair<Timestamp, std::string> &value);
    int Get(uint64_t id, const std::string &key, const Timestamp &timestamp, 
        std::pair<Timestamp, std::string> &value);
    
    int Get(uint64_t id, const std::string &key,
        std::pair<Timestamp, std::string> &value, uint64_t &dependency);
    int Get(uint64_t id, const std::string &key, const Timestamp &timestamp, 
        std::pair<Timestamp, std::string> &value, uint64_t &dependency);
    int Prepare(uint64_t id, const Transaction &txn, bool do_check);
    std::vector<uint64_t> Commit(uint64_t id, uint64_t timestamp);
    std::vector<uint64_t> Abort(uint64_t id, const Transaction &txn);
    void Load(const std::string &key, const std::string &value,
        const Timestamp &timestamp);
    void PreCommit(uint64_t id);

private:
    // Data store.
    VersionedKVStore store;

    // Locks manager.
    LockServer locks;

    std::map<uint64_t, Transaction> prepared;

    void dropLocks(uint64_t id, const Transaction &txn);
    int getLocks(uint64_t id, const Transaction &txn);
};

} // namespace strongstore

#endif /* _STRONG_LOCK_STORE_H_ */
