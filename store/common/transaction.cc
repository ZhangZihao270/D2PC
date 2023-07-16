// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/transaction.cc
 *   A transaction implementation.
 *
 **********************************************************************/

#include "store/common/transaction.h"

using namespace std;

Transaction::Transaction() :
    id(0), readSet(), writeSet(), in(0), out() { }

Transaction::Transaction(uint64_t id) :
    id(id), readSet(), writeSet(), in(0), out(){ }

Transaction::Transaction(uint64_t id, uint64_t timestamp, uint64_t replica_id) :
    id(id), readSet(), writeSet(), in(0), out(), commitTs(Timestamp(timestamp, replica_id)){ }

Transaction::Transaction(uint64_t id, uint64_t timestamp, uint64_t replica_id, uint64_t primary_shard) :
    id(id), primary_shard(primary_shard), readSet(), writeSet(), in(0), out(), commitTs(Timestamp(timestamp, replica_id)){ }

Transaction::Transaction(const TransactionMessage &msg) 
{
    for (int i = 0; i < msg.readset_size(); i++) {
        ReadMessage readMsg = msg.readset(i);
        readSet[readMsg.key()] = Timestamp(readMsg.readtime());
    }

    for (int i = 0; i < msg.writeset_size(); i++) {
        WriteMessage writeMsg = msg.writeset(i);
        writeSet[writeMsg.key()] = writeMsg.value();
    }
}

Transaction::~Transaction() { }

void 
Transaction::setCommitTs(uint64_t replica, uint64_t ts){
    commitTs = Timestamp(ts, replica);
}

void 
Transaction::setPrimaryShard(int shard){
    primary_shard = shard;
}

const unordered_map<string, Timestamp>&
Transaction::getReadSet() const
{
    return readSet;
}

const unordered_map<string, string>&
Transaction::getWriteSet() const
{
    return writeSet;
}

const std::vector<uint64_t>& 
Transaction::getParticipants() const{
    return participants;
}

const uint64_t 
Transaction::getID() const
{
    return id;
}

const Timestamp 
Transaction::getTimestamp() const
{
    return commitTs;
}

const uint64_t 
Transaction::getPrimaryShard() const{
    return primary_shard;
}

void
Transaction::addReadSet(const string &key,
                        const Timestamp &readTime)
{
    readSet[key] = readTime;
}

void
Transaction::addWriteSet(const string &key,
                         const string &value)
{
    writeSet[key] = value;
}

void 
Transaction::addParticipant(uint64_t p){
    participants.push_back(p);
}

void
Transaction::serialize(TransactionMessage *msg) const
{
    for (auto read : readSet) {
        ReadMessage *readMsg = msg->add_readset();
        readMsg->set_key(read.first);
        read.second.serialize(readMsg->mutable_readtime());
    }

    for (auto write : writeSet) {
        WriteMessage *writeMsg = msg->add_writeset();
        writeMsg->set_key(write.first);
        writeMsg->set_value(write.second);
    }
}