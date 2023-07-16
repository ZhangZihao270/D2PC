#include "replication/lr/log.h"

#include <utility>

#include "lib/assert.h"

namespace replication{
namespace lr{



void 
LogEntry::addWriteSet(std::string key, std::string value){
    wset[key] = value;
}

void
LogEntry::setState(proto::LogEntryState newstate){
    state = newstate;
}

Log::Log(lsn start) : start(start), last(start) {};

Log::Log(const proto::ReplicaLogProto &log_proto, lsn start) : start(start){
    for(const proto::LogEntryProto &entry_proto : log_proto.entry()){
        const term_t term = entry_proto.term();
        const uint64_t timestamp = entry_proto.timestamp();
        // const uint64_t replicaid = entry_proto.replicaid();
        const proto::LogEntryState state = entry_proto.state();
        const lsn id = entry_proto.lsn();
        const txn_id txn = entry_proto.txnid();
        LogEntry entry = LogEntry(term, id, txn, timestamp);
        entry.setState(state);
        for(const proto::WriteSet &wsetEntry : entry_proto.wset()){
            entry.addWriteSet(wsetEntry.key(), wsetEntry.value());
        }
        Add(entry);
    }
}

LogEntry*
Log::Add(const LogEntry& entry){
    ASSERT(entries.count(entry.id) == 0)

    // 
    ASSERT(entry.id > lastlsn);


    entries[entry.id] = entry;
    storedEntries.push_back(entry.id);
    return & entries[entry.id];
}



LogEntry*
Log::Add(term_t term, txn_id txn, std::unordered_map<std::string, std::string> wset,
        proto::LogEntryState state, uint64_t ts){
    lsn id = last++;
    return Add(LogEntry(term, id, txn, wset, state, ts));        
}



LogEntry*
Log::Add(const proto::LogEntryProto &entry_proto){
    const term_t term = entry_proto.term();
    // const uint64_t replicaid = entry_proto.replicaid();
    const proto::LogEntryState state = entry_proto.state();
    const lsn id = entry_proto.lsn();
    const txn_id txn = entry_proto.txnid();
    const uint64_t ts = entry_proto.timestamp();
    LogEntry entry = LogEntry(term, id, txn, ts);
    entry.setState(state);
    for(const proto::WriteSet &wsetEntry : entry_proto.wset()){
        entry.addWriteSet(wsetEntry.key(), wsetEntry.value());
    }
    return Add(entry);
}

LogEntry*
Log::Find(lsn id){
    if(entries.empty() || entries.count(id) == 0){
        return NULL;
    }

    LogEntry* entry = &entries[id];
    ASSERT(entry->id == id);
    return entry;
}

bool
Log::SetStatus(lsn id, proto::LogEntryState state){
    LogEntry *entry = Find(id);
    if(entry == NULL){
        return false;
    }

    entry->setState(state);
    return true;
}

LogEntry * 
Log::Last(){
    if(entries.empty()){
        return NULL;
    }

    lsn lastlsn = storedEntries.back();
    return Find(lastlsn);
}

uint64_t 
Log::LastLsn() const{
    if (entries.empty()){
        return start-1;
    } else {
        return storedEntries.back();
    }
}

uint64_t
Log::FirstLsn() const{
    return storedEntries.front();
}

bool
Log::Empty() const{
    return entries.empty();
}

void 
Log::Remove(lsn id){
    entries.erase(id);
}

void
Log::ToProto(proto::ReplicaLogProto *proto) const{
    for(const std::pair<const lsn, LogEntry> &p :entries){
        const LogEntry &entry = p.second;
        proto::LogEntryProto *entry_proto = proto->add_entry();

        entry_proto->set_term(entry.term);
        // entry_proto->set_replicaid(entry.ts.getID());
        entry_proto->set_lsn(entry.id);
        entry_proto->set_txnid(entry.txn);
        entry_proto->set_state(entry.state);
        entry_proto->set_timestamp(entry.ts);

        for(const std::pair<std::string, std::string> &wp : entry.wset){
            proto::WriteSet *wsetEntry = entry_proto->add_wset();

            wsetEntry->set_key(wp.first);
            wsetEntry->set_value(wp.second);
        }
    }
}

void
Log::EntryToProto(proto::LogEntryProto *entry_proto, lsn id){
    LogEntry &entry = entries[id];

    entry_proto->set_term(entry.term);
    // entry_proto->set_replicaid(entry.ts.getID());
    entry_proto->set_lsn(entry.id);
    entry_proto->set_txnid(entry.txn);
    entry_proto->set_state(entry.state);
    entry_proto->set_timestamp(entry.ts);

    for(const std::pair<std::string, std::string> &wp : entry.wset){
        proto::WriteSet *wsetEntry = entry_proto->add_wset();

        wsetEntry->set_key(wp.first);
        wsetEntry->set_value(wp.second);
    }
}

void
Log::EntryToProto(proto::LogEntryProto *entry_proto, LogEntry entry){
    entry_proto->set_term(entry.term);
    entry_proto->set_lsn(entry.id);
    entry_proto->set_txnid(entry.txn);
    entry_proto->set_state(entry.state);
    entry_proto->set_timestamp(entry.ts);

    for(const std::pair<std::string, std::string> &wp : entry.wset){
        proto::WriteSet *wsetEntry = entry_proto->add_wset();

        wsetEntry->set_key(wp.first);
        wsetEntry->set_value(wp.second);
    }
}

const std::map<lsn, LogEntry> &
Log::Entries() const{
    return entries;
}

}
}