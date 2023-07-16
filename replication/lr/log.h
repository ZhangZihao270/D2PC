#ifndef _LR_LOG_H_
#define _LR_LOG_H_

#include <map>
#include <string>
#include <utility>

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/common/request.pb.h"
#include "replication/common/viewstamp.h"
#include "replication/lr/lr-proto.pb.h"
#include "store/common/timestamp.h"

namespace replication{
namespace lr{

typedef uint64_t term_t;
typedef uint64_t lsn;
typedef uint64_t txn_id;

struct LogEntry
{
    term_t term;
    lsn id;
    txn_id txn;
    uint64_t ts;
    proto::LogEntryState state;
    std::unordered_map<std::string, std::string> wset;
    //std::unordered_map<std::string, timestamp> rset;

    LogEntry(){}
    LogEntry(term_t term, lsn id, txn_id txn, uint64_t ts) : term(term), id(id), txn(txn), ts(ts){}
    LogEntry(term_t term, lsn id, txn_id txn, std::unordered_map<std::string, std::string> wset,
        proto::LogEntryState state, uint64_t ts) : 
        term(term), id(id), txn(txn), wset(wset), state(state), ts(ts){}
    LogEntry(term_t term, txn_id txn, std::unordered_map<std::string, std::string> wset,
        proto::LogEntryState state) : 
        term(term), txn(txn), wset(wset), state(state){}
    LogEntry(const LogEntry & x)
        : term(x.term), id(x.id), txn(txn), wset(x.wset), state(x.state), ts(x.ts){}
    
    // LogEntry(){}
    // LogEntry(term_t term, Timestamp ts, lsn id, txn_id txn) : term(term), ts(ts), id(id), txn(txn){}
    // LogEntry(term_t term, Timestamp ts, lsn id, txn_id txn, std::unordered_map<std::string, std::string> wset,
    //     proto::LogEntryState state) : 
    //     term(term), ts(ts), id(id), txn(txn), wset(wset), state(state){}
    // LogEntry(const LogEntry & x)
    //     : term(x.term), ts(x.ts), id(x.id), txn(txn), wset(x.wset), state(x.state){}
    
    //void addReadSet(std::string key, Timestamp version);
    void addWriteSet(std::string key, std::string value);
    void setState(proto::LogEntryState state);
    
    virtual ~LogEntry(){};
};

class Log{
public:
    Log(lsn start);
    Log(const proto::ReplicaLogProto &log, lsn start);
    // Log(Log &&other) : Log() {swap(*this, other);}
    // Log(const Log &) = delete;
    // Log &operator=(const Log &) = delete;
    // Log &operator=(Log &&other) {
    //     swap(*this, other);
    //     return *this;
    // }
    // friend void swap(Log &x, Log &y) {
    //     std::swap(x.entries, y.entries);
    // }

    
    // LogEntry &Add(term_t term, Timestamp ts, lsn id, txn_id txn);
    // LogEntry &Add(term_t term, Timestamp ts, lsn id, txn_id txn, std::unordered_map<std::string, std::string> wset,
    //     proto::LogEntryState state);

    // LogEntry &Add(term_t term, lsn id, txn_id txn);
    // LogEntry &Add(term_t term, lsn id, txn_id txn, std::unordered_map<std::string, std::string> wset,
    //     proto::LogEntryState state);

    // for leader
    LogEntry * Add(term_t term, txn_id txn, std::unordered_map<std::string, std::string> wset,
        proto::LogEntryState state, uint64_t ts);

    // for follower
    LogEntry * Add(const LogEntry& entry);
    LogEntry * Add(const proto::LogEntryProto &entry_proto);

    LogEntry *Find(lsn id);
    bool SetStatus(lsn id, proto::LogEntryState state);
    LogEntry * Last();
    uint64_t LastLsn() const;
    uint64_t FirstLsn() const;
    void Remove(lsn id);
    void RemoveAfter(lsn id);
    bool Empty() const;
    void ToProto(proto::ReplicaLogProto *proto) const;
    void EntryToProto(proto::LogEntryProto *entry_proto, lsn id);
    void EntryToProto(proto::LogEntryProto *entry_proto, LogEntry entry);
    const std::map<lsn, LogEntry> &Entries() const;
private:
    std::map<lsn, LogEntry> entries;
    std::vector<lsn> storedEntries;
    lsn start;
    lsn last;
    bool is_replica;
};

}
}
#endif