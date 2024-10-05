

#ifndef _STRONG_OCC_STORE_H_
#define _STRONG_OCC_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/backend/versionstore.h"
#include "store/common/backend/txnstore.h"
#include "store/common/transaction.h"

#include <map>

namespace strongstore {

class OCCStore : public TxnStore
{
public:
    OCCStore();
    ~OCCStore();

    // Overriding from TxnStore.
    int Get(uint64_t id, const std::string &key, std::pair<Timestamp, std::string> &value);
    int Get(uint64_t id, const std::string &key, const Timestamp &timestamp, 
            std::pair<Timestamp, std::string> &value);

    int Get(uint64_t id, const std::string &key, std::pair<Timestamp, std::string> &value, 
            uint64_t &dependency);
    int Get(uint64_t id, const std::string &key, const Timestamp &timestamp, 
            std::pair<Timestamp, std::string> &value, uint64_t &dependency);
    int Prepare(uint64_t id, const Transaction &txn, bool do_check);
    void Commit(uint64_t id, uint64_t timestamp);
    void Abort(uint64_t id, const Transaction &txn = Transaction());
    void Load(const std::string &key, const std::string &value, const Timestamp &timestamp);
    void PreCommit(uint64_t id);

private:
    // Data store.
    VersionedKVStore store;

    std::map<uint64_t, Transaction> prepared;
    std::map<uint64_t, Transaction> precommitted;

    std::set<std::string> getPreparedWrites();
    std::set<std::string> getPreparedReadWrites();
};

} // namespace strongstore

#endif /* _STRONG_OCC_STORE_H_ */
