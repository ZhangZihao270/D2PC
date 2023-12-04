

#ifndef _VERSIONED_KV_STORE_H_
#define _VERSIONED_KV_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"

#include <set>
#include <map>
#include <unordered_map>

class VersionedKVStore
{
public:
    VersionedKVStore();
    ~VersionedKVStore();

    bool get(const std::string &key, std::pair<Timestamp, std::string> &value);
    bool get(const std::string &key, const Timestamp &t, std::pair<Timestamp, std::string> &value);
    bool get(const std::string &key, std::pair<Timestamp, std::string> &value, uint64_t &tid);
    bool getRange(const std::string &key, const Timestamp &t, std::pair<Timestamp, Timestamp> &range);
    bool getLastRead(const std::string &key, Timestamp &readTime);
    bool getLastRead(const std::string &key, const Timestamp &t, Timestamp &readTime);
    void put(const std::string &key, const std::string &value, const Timestamp &t);
    void commitGet(const std::string &key, const Timestamp &readTime, const Timestamp &commit);
    void preCommit(const std::string &key, uint64_t tid);
    uint64_t commit(const std::string &key, uint64_t tid);
    bool hasPreCommit(const std::string &key, uint64_t tid);

private:
    struct VersionedValue {
        Timestamp write;
        std::string value;
        // std::vector<uint64_t> precommits;

        VersionedValue(Timestamp commit) : write(commit), value("tmp") { };
        VersionedValue(Timestamp commit, std::string val) : write(commit), value(val) { };

        friend bool operator> (const VersionedValue &v1, const VersionedValue &v2) {
            return v1.write > v2.write;
        };
        friend bool operator< (const VersionedValue &v1, const VersionedValue &v2) {
            return v1.write < v2.write;
        };
    };

    /* Global store which keeps key -> (timestamp, value) list. */
    std::unordered_map< std::string, std::set<VersionedValue> > store;
    std::unordered_map< std::string, std::map< Timestamp, Timestamp > > lastReads;
    std::unordered_map< std::string, std::vector<uint64_t> > precommits;
    std::unordered_map< std::string, std::vector<uint64_t> > blocked;

    bool inStore(const std::string &key);
    void getValue(const std::string &key, const Timestamp &t, std::set<VersionedValue>::iterator &it);
};

#endif  /* _VERSIONED_KV_STORE_H_ */
