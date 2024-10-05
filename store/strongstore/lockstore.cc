

#include "store/strongstore/lockstore.h"

using namespace std;

namespace strongstore {

LockStore::LockStore() : TxnStore(), store() { }
LockStore::~LockStore() { }

/* If read is executed on the closest replica which is not leader, 
* it should not require lock.
* This Get Function will acquire lock, but it not violate the correctness,
* since no write operations is executed on this replica. The lock will not
* block other transactions.
* TODO if the attached replica is not leader, the get does not acquire lock.
*/
int
LockStore::Get(uint64_t id, const string &key, pair<Timestamp, string> &value)
{
    Debug("[%lu] GET %s", id, key.c_str());
    // string val;
    // std::pair<Timestamp, std::string> val;

    if (!store.get(key, value)) {
        // couldn't find the key
        return REPLY_FAIL;
    }

    // grab the lock (ok, if we already have it)
    if (locks.lockForRead(key, id)) {
        Debug("acquire read lock");
        // value = make_pair(Timestamp(), val);
        return REPLY_OK;
    } else {
        Debug("[%lu] Could not acquire read lock", id);
        return REPLY_RETRY;
    }
}

int
LockStore::Get(uint64_t id, const string &key, const Timestamp &timestamp, 
                pair<Timestamp, string> &value)
{
    return Get(id, key, value);
}

int
LockStore::Get(uint64_t id, const string &key, pair<Timestamp, string> &value, uint64_t &dependency)
{
    Debug("[%lu] GET %s", id, key.c_str());

    if (store.get(key, value, dependency)) {
        // couldn't find the key
        if (dependency > 0){
            Debug("[%lu] read depend on [%lu]", dependency);
        }
        Debug("[%lu] GET %s %lu", id, key.c_str(), value.first.getTimestamp());
        // Debug("value: %s", value.second.c_str());
        return REPLY_OK;
    } else {
        Debug("[%lu] GET %s failed", id, key.c_str());
        return REPLY_FAIL;
    }
}

int
LockStore::Get(uint64_t id, const string &key, const Timestamp &timestamp, 
                pair<Timestamp, string> &value, uint64_t &dependency)
{
    return Get(id, key, value, dependency);
}


int
LockStore::Prepare(uint64_t id, const Transaction &txn, bool do_check)
{    
    Debug("[%lu] START PREPARE", id);

    if (prepared.size() > 100) {
        Warning("Lots of prepared transactions! %lu", prepared.size());
    }

    if (prepared.find(id) != prepared.end()) {
        Debug("[%lu] Already prepared", id);
        return REPLY_OK;
    }

    if(do_check){
        int ret = getLocks(id, txn);
        if (ret == REPLY_OK) {
            prepared[id] = txn;
            Debug("[%lu] PREPARED TO COMMIT", id);
        } else {
            Debug("[%lu] Could not acquire write locks", id); 
        }
        return ret;
    } else {
        prepared[id] = txn;
        return REPLY_OK;
    }
}

void
LockStore::Commit(uint64_t id, uint64_t timestamp)
{
    Debug("[%lu] COMMIT", id);
    // ASSERT(prepared.find(id) != prepared.end());

    if (precommitted.find(id) != precommitted.end()){
        Transaction txn = precommitted[id];

        for (auto &write : txn.getWriteSet()) {
            store.put(write.first, write.second, timestamp);
            store.commit(write.first, id);
        }

        precommitted.erase(id);
    
        Debug("Commit, Precommit list size: %d", precommitted.size());
    } else if (prepared.find(id)!=prepared.end()){
        Transaction txn = prepared[id];

        for (auto &write : txn.getWriteSet()) {
            store.put(write.first, write.second, timestamp);
            store.commit(write.first, id);
        }
        // Drop locks.
        dropLocks(id, txn);

        prepared.erase(id);
    }
}

// for parallel mode, to release locks and adds tid to each write key's precommit list
void 
LockStore::PreCommit(uint64_t id){
    Debug("[%lu] PRE COMMIT", id);
    ASSERT(prepared.find(id) != prepared.end());

    Transaction txn = prepared[id];

    for (auto &write : txn.getWriteSet()) {
        store.preCommit(write.first, id);
    }

    // Drop locks.
    dropLocks(id, txn);
    prepared.erase(id);
    precommitted[id] = txn;

    Debug("PreCommit, Prepared size: %d", prepared.size());
}

void
LockStore::Abort(uint64_t id, const Transaction &txn)
{
    Debug("[%lu] ABORT", id);

    if (precommitted.find(id) != precommitted.end()){
        Transaction txn = precommitted[id];
        precommitted.erase(id);
    } else if (prepared.find(id)!=prepared.end()) {
        Transaction txn = prepared[id];
        dropLocks(id, txn);
        prepared.erase(id);
    }

    // return nexts;
}

void
LockStore::Load(const string &key, const string &value, const Timestamp &timestamp)
{
    store.put(key, value, timestamp);
}

/* Used on commit and abort for second phase of 2PL. */
void
LockStore::dropLocks(uint64_t id, const Transaction &txn)
{
    for (auto &write : txn.getWriteSet()) {
        locks.releaseForWrite(write.first, id);
    }

    for (auto &read : txn.getReadSet()) {
        locks.releaseForRead(read.first, id);
    }
}

int
LockStore::getLocks(uint64_t id, const Transaction &txn)
{
    int ret = 0;
    // if we don't have read locks, get read locks
    for (auto &read : txn.getReadSet()) {
        // first check if read stale data
        std::pair<Timestamp, std::string> cur;
        bool res = store.get(read.first, cur);

        // ASSERT(ret);
        if (!res)
            continue;

        // If this key has been written since we read it, abort.
        if (cur.first > read.second) {
            Debug("[%lu] ABORT rw conflict key:%s %lu %lu",
                id, read.first.c_str(), cur.first.getTimestamp(),
                read.second.getTimestamp());
                
            Abort(id, txn);
            return REPLY_ABORT;
        }
        
        if (!locks.lockForRead(read.first, id)) {
            ret = REPLY_FAIL;
        }
    }
    for (auto &write : txn.getWriteSet()) {
        if (!locks.lockForWrite(write.first, id)) {
            ret = REPLY_FAIL;
        }
    }
    return ret;
}



} // namespace strongstore
