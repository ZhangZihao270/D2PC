

#ifndef _BUFFER_CLIENT_H_
#define _BUFFER_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/promise.h"
#include "store/common/transaction.h"
#include "store/common/frontend/txnclient.h"

#include <unordered_map>

class BufferClient
{
public:
    BufferClient(TxnClient *txnclient);
    ~BufferClient();

    // Begin a transaction with given tid.
    void Begin(uint64_t tid);

    // Get value corresponding to key.
    void Get(const string &key, Promise *promise = NULL);

    // Put value for given key.
    void Put(const string &key, const string &value, Promise *promise = NULL);

    // Prepare (Spanner requires a prepare timestamp)
    void Prepare(const Timestamp &timestamp = Timestamp(), Promise *promise = NULL); 

    void ParallelModeCommit(Promise *promise);

    // Commit the ongoing transaction.
    void Commit(uint64_t timestamp = 0, Promise *promise = NULL);

    // Abort the running transaction.
    void Abort(Promise *promise = NULL);

    void SetParticipants(std::vector<int> participants) {
        for(auto p : participants){
            txn.addParticipant(p);
        }
    }

    void SetPrimaryShard(int shard){
        txn.setPrimaryShard(shard);
    }

    void AddReadSet(const std::string &key, const Timestamp &readTime){
        txn.addReadSet(key, readTime);
    }

    void AddWriteSet(const std::string &key, const std::string &value){
        txn.addWriteSet(key, value);
    }

    const std::unordered_map<string, Timestamp>& GetReadSet(){
        return txn.getReadSet();
    }

    const std::unordered_map<string, string>& GetWriteSet(){
        return txn.getWriteSet();
    }

private:
    // Underlying single shard transaction client implementation.
    TxnClient* txnclient;

    // Transaction to keep track of read and write set.
    Transaction txn;

    // Unique transaction id to keep track of ongoing transaction.
    uint64_t tid;
};

#endif /* _BUFFER_CLIENT_H_ */
