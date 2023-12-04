// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/transaction.h:
 *   A Transaction representation.
 *
 **********************************************************************/

#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#include <sys/time.h>
#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"
#include "store/common/common-proto.pb.h"

#include <unordered_map>

// Reply types
#define REPLY_OK 0
#define REPLY_FAIL 1
#define REPLY_RETRY 2
#define REPLY_ABSTAIN 3
#define REPLY_TIMEOUT 4
#define REPLY_NETWORK_FAILURE 5
#define REPLY_MAX 6
#define REPLY_ABORT 7
#define REPLY_HAS_PRECOMMIT 8

enum Mode {
    MODE_UNKNOWN,
    MODE_OCC,
    MODE_LOCK,
    MODE_SPAN_OCC,
    MODE_SPAN_LOCK
};

enum TpcMode {
    MODE_FAST,
    MODE_SLOW,
    MODE_PARALLEL,
    MODE_RC,
    MODE_CAROUSEL
};

class Transaction {
private:
    // map between key and value(s)
    std::unordered_map<std::string, std::string> writeSet;

    uint64_t id;

    // The following are for OCC mode.
    Timestamp startTs;
    Timestamp commitTs;
    // map between key and timestamp at
    // which the read happened and how
    // many times this key has been read
    std::unordered_map<std::string, Timestamp> readSet;
    std::vector<uint64_t> participants;
    uint64_t primary_shard;

    std::vector<uint64_t> out;
    uint64_t in;

    struct timeval t1, t2;

public:
    Transaction();
    Transaction(uint64_t id);
    Transaction(uint64_t id, uint64_t timestamp, uint64_t replica_id);
    Transaction(uint64_t id, uint64_t timestamp, uint64_t replica_id, uint64_t primary_shard);
    Transaction(const TransactionMessage &msg);
    ~Transaction();

    const std::unordered_map<std::string, Timestamp>& getReadSet() const;
    const std::unordered_map<std::string, std::string>& getWriteSet() const;
    const std::vector<uint64_t>& getParticipants() const;
    const uint64_t getID() const;
    const Timestamp getTimestamp() const;
    const uint64_t getPrimaryShard() const;

    void setCommitTs(uint64_t replica, uint64_t ts);
    void setPrimaryShard(int shard);

    void increaseIn(){in++;}
    void decreaseIn(){in--;}
    void setIn(uint64_t i){in = i;}
    void addOut(uint64_t tid){out.push_back(tid);}
    const std::vector<uint64_t>& getOut() const{return out;}

    
    void addReadSet(const std::string &key, const Timestamp &readTime);
    void addWriteSet(const std::string &key, const std::string &value);
    void serialize(TransactionMessage *msg) const;
    void addParticipant(uint64_t p);

    void getStartTimeOfCriticalPath(){gettimeofday(&t1, NULL);}
    void getEndTimeOfCriticalPath(){gettimeofday(&t2, NULL);}
    long calculateCriticalPathLen(){
        return (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);} 
};

#endif /* _TRANSACTION_H_ */
