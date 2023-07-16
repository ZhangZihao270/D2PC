

#ifndef _STRONG_SHARDCLIENT_H_
#define _STRONG_SHARDCLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/lr/client.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/txnclient.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/strongstore/strong-proto.pb.h"

using namespace replication::lr;

namespace strongstore {

enum Mode {
    MODE_UNKNOWN,
    MODE_OCC,
    MODE_LOCK,
    MODE_SPAN_OCC,
    MODE_SPAN_LOCK
};

class ShardClient : public TxnClient
{
public:
    /* Constructor needs path to shard config. */
    ShardClient(Mode mode,
        TpcMode tpcMode,
        const std::string &configPath, 
        Transport *transport,
        uint64_t client_id,
        int shard,
        int closestReplica);
    ~ShardClient();

    // Overriding from TxnClient
    void Begin(uint64_t id);
    void Get(uint64_t id,
            const std::string &key,
            Promise *promise = NULL);
    void Get(uint64_t id,
            const std::string &key,
            const Timestamp &timestamp, 
            Promise *promise = NULL);
    void Put(uint64_t id,
	     const std::string &key,
	     const std::string &value,
	     Promise *promise = NULL);
    void Prepare(uint64_t id, 
                 const Transaction &txn,
                 const Timestamp &timestamp = Timestamp(),
                 Promise *promise = NULL);
    void Commit(uint64_t id, 
                const Transaction &txn,
                uint64_t timestamp,
                Promise *promise = NULL);
    void Abort(uint64_t id, 
               const Transaction &txn,
               Promise *promise = NULL);
    void ParallelModeCommit(uint64_t id, const Transaction &txn, Promise *promise = NULL);

private:
    Transport *transport; // Transport layer.
    uint64_t client_id; // Unique ID for this client.
    int shard; // which shard this client accesses
    int replica; // which replica to use for reads
    int leader;

    TpcMode tpcMode;

    // replication::vr::VRClient *client; // Client proxy.
    replication::lr::LRClient *client;
    Promise *waiting; // waiting thread
    // Promise *parallelmode_waiting;
    Promise *blockingBegin; // block until finished 

    /* Timeout for Get requests, which only go to one replica. */
    void GetTimeout(uint64_t tid);
    void PrepareTimeout(uint64_t tid);

    /* Callbacks for hearing back from a shard for an operation. */
    // void GetCallback(const std::string &, const std::string &);
    // void PrepareCallback(const std::string &, const std::string &);
    void CommitCallback(const std::string &, const std::string &);
    void AbortCallback(const std::string &, const std::string &);

    void GetCallback(std::vector<std::pair<string, uint64_t>> readKey, 
                    std::vector<std::string> values, int state);
    void PrepareCallback(const uint64_t tid, const uint64_t shard, int state, uint64_t timestamp);
    void ParallelModeCommitCallback(const uint64_t tid, int state, uint64_t timestamp);

    /* Helper Functions for starting and finishing requests */
    void StartRequest();
    void WaitForResponse();
    void FinishRequest(const std::string &reply_str);
    void FinishRequest();
    int SendGet(const std::string &request_str);

};

} // namespace strongstore

#endif /* _STRONG_SHARDCLIENT_H_ */
