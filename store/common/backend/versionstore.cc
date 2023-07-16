

#include "store/common/backend/versionstore.h"

using namespace std;

VersionedKVStore::VersionedKVStore() { }
    
VersionedKVStore::~VersionedKVStore() { }

bool
VersionedKVStore::inStore(const string &key)
{
    return store.find(key) != store.end() && store[key].size() > 0;
}

void
VersionedKVStore::getValue(const string &key, const Timestamp &t, set<VersionedKVStore::VersionedValue>::iterator &it)
{
    VersionedValue v(t);
    it = store[key].upper_bound(v);

    // if there is no valid version at this timestamp
    if (it == store[key].begin()) {
        it = store[key].end();
    } else {
        it--;
    }
}


/* Returns the most recent value and timestamp for given key.
 * Error if key does not exist. */
bool
VersionedKVStore::get(const string &key, pair<Timestamp, string> &value)
{
    // check for existence of key in store
    if (inStore(key)) {
        // Debug("Find the key.");
        VersionedValue v = *(store[key].rbegin());
        value = make_pair(v.write, v.value);
        return true;
    }
    // Debug("the key is not in the store");
    return false;
}

/*
* For Parallel mode, if the read key has procommit txns, it will get the last precommit txn.
*/
bool
VersionedKVStore::get(const string &key, pair<Timestamp, string> &value, uint64_t &tid)
{
    Debug("Key size: %lu", store.size());
    Debug("Find the key %s", key.c_str());
    // check for existence of key in store
    if (inStore(key)) {
        if(precommits.find(key) == precommits.end() || precommits[key].empty()){
            VersionedValue v = *(store[key].rbegin());
            value = make_pair(v.write, v.value);
            tid = 0;
        } else {
            tid = precommits[key].back();
        }
        return true;
    }
    Debug("the key is not in the store");
    return false;
}
    
/* Returns the value valid at given timestamp.
 * Error if key did not exist at the timestamp. */
bool
VersionedKVStore::get(const string &key, const Timestamp &t, pair<Timestamp, string> &value)
{
    if (inStore(key)) {
        set<VersionedValue>::iterator it;
        getValue(key, t, it);
        if (it != store[key].end()) {
            value = make_pair((*it).write, (*it).value);
            return true;
        }
    }
    return false;
}

bool
VersionedKVStore::getRange(const string &key, const Timestamp &t,
			   pair<Timestamp, Timestamp> &range)
{
    if (inStore(key)) {
        set<VersionedValue>::iterator it;
        getValue(key, t, it);

        if (it != store[key].end()) {
            range.first = (*it).write;
            it++;
            if (it != store[key].end()) {
                range.second = (*it).write;
            }
            return true;
        }
    }
    return false;
}

void
VersionedKVStore::put(const string &key, const string &value, const Timestamp &t)
{
    // Key does not exist. Create a list and an entry.
    store[key].insert(VersionedValue(t, value));
}

/*
 * Commit a read by updating the timestamp of the latest read txn for
 * the version of the key that the txn read.
 */
void
VersionedKVStore::commitGet(const string &key, const Timestamp &readTime, const Timestamp &commit)
{
    // Hmm ... could read a key we don't have if we are behind ... do we commit this or wait for the log update?
    if (inStore(key)) {
        set<VersionedValue>::iterator it;
        getValue(key, readTime, it);
        
        if (it != store[key].end()) {
            // figure out if anyone has read this version before
            if (lastReads.find(key) != lastReads.end() &&
                lastReads[key].find((*it).write) != lastReads[key].end()) {
                if (lastReads[key][(*it).write] < commit) {
                    lastReads[key][(*it).write] = commit;
                }
            }
        }
    } // otherwise, ignore the read
}

/*
 * Precommit a transaction, precommit means end transaction critical path early.
* add tid to key's precommit
*/
// 
void 
VersionedKVStore::preCommit(const std::string &key, const uint64_t tid){
    // check for existence of key in store
    if (inStore(key)) {
        // Debug("Find the key.");
        precommits[key].push_back(tid);
    }
}

/*
 * Commit a transaction, remove tid from key's precommit
*/
void 
VersionedKVStore::commit(const std::string &key, const uint64_t tid){
    if (inStore(key)) {
        // Debug("Find the key.");
        std::vector<uint64_t>::iterator it;
        Debug("Check if has precommit txns");
        if(precommits.find(key) != precommits.end()){
            Debug("Precommit num: %lu", precommits[key].size());
            for(auto it = precommits[key].begin(); it != precommits[key].end();){
                if(*it == tid){
                    Debug("Find the precommit txn");
                    it = precommits[key].erase(it);
                    Debug("Earse it");
                } else {
                    ++it;
                }
            }
        }
    }
}

bool
VersionedKVStore::getLastRead(const string &key, Timestamp &lastRead)
{
    if (inStore(key)) {
        VersionedValue v = *(store[key].rbegin());
        if (lastReads.find(key) != lastReads.end() &&
            lastReads[key].find(v.write) != lastReads[key].end()) {
            lastRead = lastReads[key][v.write];
            return true;
        }
    }
    return false;
}    

/*
 * Get the latest read for the write valid at timestamp t
 */
bool
VersionedKVStore::getLastRead(const string &key, const Timestamp &t, Timestamp &lastRead)
{
    if (inStore(key)) {
        set<VersionedValue>::iterator it;
        getValue(key, t, it);
        ASSERT(it != store[key].end());

        // figure out if anyone has read this version before
        if (lastReads.find(key) != lastReads.end() &&
            lastReads[key].find((*it).write) != lastReads[key].end()) {
            lastRead = lastReads[key][(*it).write];
            return true;
        }
    }
    return false;	
}

