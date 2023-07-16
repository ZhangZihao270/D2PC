

#include "store/strongstore/occstore.h"

namespace strongstore {

using namespace std;

OCCStore::OCCStore() : store() { }
OCCStore::~OCCStore() { }

int
OCCStore::Get(uint64_t id, const string &key, pair<Timestamp, string> &value)
{
    Debug("[%lu] GET %s", id, key.c_str());

    // Get latest from store
    if (store.get(key, value)) {
        Debug("[%lu] GET %s %lu", id, key.c_str(), value.first.getTimestamp());
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

int
OCCStore::Get(uint64_t id, const string &key, const Timestamp &timestamp, 
            pair<Timestamp, string> &value)
{
    Debug("[%lu] GET %s", id, key.c_str());
    
    // Get version at timestamp from store
    if (store.get(key, timestamp, value)) {
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

int
OCCStore::Get(uint64_t id, const string &key, pair<Timestamp, string> &value, uint64_t &dependency)
{
    Debug("[%lu] GET %s", id, key.c_str());

    // Get latest from store
    if (store.get(key, value, dependency)) {
        Debug("[%lu] GET %s %lu", id, key.c_str(), value.first.getTimestamp());
        return REPLY_OK;
    } else {
        Debug("[%lu] GET %s failed", id, key.c_str());
        return REPLY_FAIL;
    }
}

int
OCCStore::Get(uint64_t id, const string &key, const Timestamp &timestamp, 
            pair<Timestamp, string> &value, uint64_t &dependency)
{
    Debug("[%lu] GET %s", id, key.c_str());
    
    // Get version at timestamp from store
    if (store.get(key, timestamp, value)) {
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

// Leader replica will do occ check, or in the Carousel mode, all replica do the check.
int
OCCStore::Prepare(uint64_t id, const Transaction &txn, bool do_check)
{    
    Debug("[%lu] START PREPARE", id);

    if (prepared.find(id) != prepared.end()) {
        Debug("[%lu] Already prepared!", id);
        return REPLY_OK;
    }

    if(do_check){
        // Do OCC checks.
        set<string> pWrites = getPreparedWrites();
        set<string> pRW = getPreparedReadWrites();

        // Check for conflicts with the read set.
        for (auto &read : txn.getReadSet()) {
            pair<Timestamp, string> cur;
            bool ret = store.get(read.first, cur);

            // ASSERT(ret);
            if (!ret)
                continue;

            // If this key has been written since we read it, abort.
            if (cur.first > read.second) {
                Debug("[%lu] ABORT rw conflict key:%s %lu %lu",
                    id, read.first.c_str(), cur.first.getTimestamp(),
                    read.second.getTimestamp());
                
                Abort(id);
                return REPLY_FAIL;
            }

            // If there is a pending write for this key, abort.
            if (pWrites.find(read.first) != pWrites.end()) {
                Debug("[%lu] ABORT rw conflict w/ prepared key:%s",
                    id, read.first.c_str());
                Abort(id);
                return REPLY_FAIL;
            }
        }

        // Check for conflicts with the write set.
        for (auto &write : txn.getWriteSet()) {
            // If there is a pending read or write for this key, abort.
            if (pRW.find(write.first) != pRW.end()) {
                Debug("[%lu] ABORT ww conflict w/ prepared key:%s", id,
                        write.first.c_str());
                Abort(id);
                return REPLY_FAIL;
            }
        }
    }

    // Otherwise, prepare this transaction for commit
    prepared[id] = txn;
    Debug("[%lu] PREPARED TO COMMIT", id);
    return REPLY_OK;
}

void
OCCStore::Commit(uint64_t id, uint64_t timestamp)
{
    Debug("[%lu] COMMIT", id);
    ASSERT(prepared.find(id) != prepared.end());

    Transaction txn = prepared[id];

    for (auto &write : txn.getWriteSet()) {
        store.put(write.first, // key
                    write.second, // value
                    Timestamp(timestamp)); // timestamp
        store.commit(write.first, id);
    }

    // prepared.erase(id);
}

// for parallel mode, to remove txn from validation list and adds tid to each write key's precommit list
void 
OCCStore::PreCommit(uint64_t id){
    Debug("[%lu] PRE COMMIT", id);
    ASSERT(prepared.find(id) != prepared.end());

    Transaction txn = prepared[id];

    for (auto &write : txn.getWriteSet()) {
        store.preCommit(write.first, id);
    }

    prepared.erase(id);
}

void
OCCStore::Abort(uint64_t id, const Transaction &txn)
{
    Debug("[%lu] ABORT", id);

    for (auto &write : txn.getWriteSet()) {
        store.commit(write.first, id);
    }

    prepared.erase(id);
}

void
OCCStore::Load(const string &key, const string &value, const Timestamp &timestamp)
{
    store.put(key, value, timestamp);
}

set<string>
OCCStore::getPreparedWrites()
{
    // gather up the set of all writes that we are currently prepared for
    set<string> writes;
    for (auto &t : prepared) {
        for (auto &write : t.second.getWriteSet()) {
            writes.insert(write.first);
        }
    }
    return writes;
}

set<string>
OCCStore::getPreparedReadWrites()
{
    // gather up the set of all writes that we are currently prepared for
    set<string> readwrites;
    for (auto &t : prepared) {
        for (auto &write : t.second.getWriteSet()) {
            readwrites.insert(write.first);
        }
        for (auto &read : t.second.getReadSet()) {
            readwrites.insert(read.first);
        }
    }
    return readwrites;
}

} // namespace strongstore
