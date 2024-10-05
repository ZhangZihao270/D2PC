

#include "store/strongstore/shardclient.h"

namespace strongstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(Mode mode, TpcMode tpcMode, const string &configPath,
                       Transport *transport, uint64_t client_id, int
                       shard, int closestReplica)
    : transport(transport), client_id(client_id), shard(shard), tpcMode(tpcMode)
{ 
    ifstream configStream(configPath);
    if (configStream.fail()) {
        fprintf(stderr, "unable to read configuration file: %s\n",
                configPath.c_str());
    }
    transport::Configuration config(configStream);

    client = new replication::lr::LRClient(config, transport, tpcMode);



    if (closestReplica == -1) {
        replica = client_id % config.n;
    } else {
        replica = closestReplica;
    }
    Debug("Sending unlogged to replica %i", replica);

    leader = shard % config.n;

    waiting = NULL;
    blockingBegin = NULL;
}

ShardClient::~ShardClient()
{ 
    delete client;
}

/* Sends BEGIN to a single shard indexed by i. */
void
ShardClient::Begin(uint64_t id)
{
    Debug("[shard %i] BEGIN: %lu", shard, id);

    // Wait for any previous pending requests.
    // if (blockingBegin != NULL) {
    //     blockingBegin->GetReply();
    //     delete blockingBegin;
    //     blockingBegin = NULL;
    // }
}

/* Returns the value corresponding to the supplied key. */
void
ShardClient::Get(uint64_t id, const std::string &key, Promise *promise)
{
    // Send the GET operation to appropriate shard.
    Debug("[shard %i] Sending GET [%s] of %lu to replica %lu", shard, key.c_str(), id, replica);
    // Debug("Promise txn id %lu", promise->GetTid());
    // // create request
    // string request_str;
    // Request request;
    // request.set_op(Request::GET);
    // request.set_txnid(id);
    // request.mutable_get()->set_key(key);
    // request.SerializeToString(&request_str);

    // set to 1 second by default
    int timeout = (promise != NULL) ? promise->GetTimeout() : 1000;

    std::vector<std::string> keys;
    keys.push_back(key);

    transport->Timer(0, [=]() {
	    waiting = promise;    
        client->InvokeGetData(id, 
                            keys, 
                            replica, 
                            bind(&ShardClient::GetCallback,
                                this,
                                placeholders::_1,
                                placeholders::_2,
                                placeholders::_3),
                            timeout,
                            bind(&ShardClient::GetTimeout,
                                this,
                                placeholders::_1)
        ); // timeout in ms
    });
}

void
ShardClient::MultiGet(uint64_t id, const std::vector<string> &keys, Promise *promise)
{
    // Send the GET operation to appropriate shard.
    Debug("[shard %i] Sending GET of %lu to replica %lu", shard, id, replica);
    Debug("Promise txn id %lu", promise->GetTid());
    // // create request
    // string request_str;
    // Request request;
    // request.set_op(Request::GET);
    // request.set_txnid(id);
    // request.mutable_get()->set_key(key);
    // request.SerializeToString(&request_str);

    // set to 1 second by default
    int timeout = (promise != NULL) ? promise->GetTimeout() : 1000;

    // std::vector<std::string> keys;
    // keys.push_back(key);

    transport->Timer(0, [=]() {
	    waiting = promise;    
        client->InvokeGetData(id, 
                            keys, 
                            replica, 
                            bind(&ShardClient::GetCallback,
                                this,
                                placeholders::_1,
                                placeholders::_2,
                                placeholders::_3),
                            timeout,
                            bind(&ShardClient::GetTimeout,
                                this,
                                placeholders::_1)
        ); // timeout in ms
    });
}

void
ShardClient::Get(uint64_t id, const string &key,
                const Timestamp &timestamp, Promise *promise)
{
    Get(id, key, promise);
}

void
ShardClient::Put(uint64_t id,
               const string &key,
               const string &value,
               Promise *promise)
{
    Panic("Unimplemented PUT");
    return;
}

void
ShardClient::Prepare(uint64_t id, const Transaction &txn,
                    const Timestamp &timestamp, Promise *promise)
{
    Debug("[shard %i] Sending PREPARE: %lu", shard, id);

    transport->Timer(0, [=]() {
	    waiting = promise;
        client->InvokePrepare(txn,
                            leader,
                            bind(&ShardClient::PrepareCallback,
                                this,
                                placeholders::_1,
                                placeholders::_2,
                                placeholders::_3,
                                placeholders::_4),
                            bind(&ShardClient::PrepareTimeout,
                                this,
                                placeholders::_1));
    });
    Debug("Send Prepare done");
}

void 
ShardClient::ParallelModeCommit(uint64_t id, const Transaction &txn, Promise *promise){
    Debug("[shard %i] Sending parallel mode commit: %lu", shard, id);

    transport->Timer(0, [=]() {
	    waiting = promise;
        client->InvokeParallelCommit(id,
                                    txn.getParticipants(),
                                    leader,
                                    bind(&ShardClient::ParallelModeCommitCallback,
                                        this,
                                        placeholders::_1,
                                        placeholders::_2,
                                        placeholders::_3));
    });
}



void
ShardClient::Commit(uint64_t id, const Transaction &txn,
                   uint64_t timestamp, Promise *promise)
{

    Debug("[shard %i] Sending COMMIT: %lu", shard, id);
    waiting = NULL;
    client->InvokeCommit(id, leader, replication::lr::proto::LogEntryState::LOG_STATE_COMMIT, nullptr);
}

/* Aborts the ongoing transaction. */
void
ShardClient::Abort(uint64_t id, const Transaction &txn, Promise *promise)
{
    Debug("[shard %i] Sending ABORT: %lu", shard, id);
    waiting = NULL;
    client->InvokeCommit(id, leader, replication::lr::proto::LogEntryState::LOG_STATE_ABORT, nullptr);
    
    // create abort request
    // string request_str;
    // Request request;
    // request.set_op(Request::ABORT);
    // request.set_txnid(id);
    // txn.serialize(request.mutable_abort()->mutable_txn());
    // request.SerializeToString(&request_str);

    // blockingBegin = new Promise(ABORT_TIMEOUT);
    // transport->Timer(0, [=]() {
	//     waiting = promise;

	//     client->Invoke(request_str,
	// 		   bind(&ShardClient::AbortCallback,
	// 			this,
	// 			placeholders::_1,
	// 			placeholders::_2));
    // });
}

void
ShardClient::GetTimeout(uint64_t tid)
{
    if (waiting != NULL) {
        Promise *w = waiting;
        waiting = NULL;
        w->Reply(REPLY_TIMEOUT);
    }
}

/* Callback from a shard replica on get operation completion. */
void
ShardClient::GetCallback(std::vector<std::pair<string, uint64_t>> readKey, std::vector<std::string> values, int state)
{
    if (waiting != NULL) {
        Promise *w = waiting;
        waiting = NULL;
        Debug("[shard %i] Received GET callback [%d] of txn %lu", shard, state, w->GetTid());
        if(!values.empty()){
            w->Reply(state, readKey[0].second, values[0]);
        }else{
            Debug("No return values");
            w->Reply(state, Timestamp(), "");
        }
    }
}
// void
// ShardClient::GetCallback(const string &request_str, const string &reply_str)
// {
//     /* Replies back from a shard. */
//     Reply reply;
//     reply.ParseFromString(reply_str);

//     Debug("[shard %i] Received GET callback [%d]", shard, reply.status());
//     if (waiting != NULL) {
//         Promise *w = waiting;
//         waiting = NULL;
//         if (reply.has_timestamp()) {
//             w->Reply(reply.status(), Timestamp(reply.timestamp()), reply.value());
//         } else {
//             w->Reply(reply.status(), reply.value());
//         }
//     }
// }

/* Callback from a shard replica on prepare operation completion. */
void
ShardClient::PrepareCallback(const uint64_t tid, const uint64_t shard, int state, uint64_t timestamp)
{
    Debug("[shard %lu] Received PREPARE callback of %lu, state [%d]", shard, tid, state);

    if(waiting == NULL || tid != waiting->GetTid() || waiting->HasReachFinalDecision()){
        return;
    }
    
    if(waiting != NULL){
        Debug("Waiting tid %lu", waiting->GetTid());
    }

    if (waiting != NULL && waiting->GetTid() == tid) {
        Promise *w = waiting;
        // waiting = NULL;
        if(tpcMode == TpcMode::MODE_SLOW || tpcMode == TpcMode::MODE_RC){
            waiting = NULL;
            if(timestamp > 0){
                w->Reply(state, Timestamp(timestamp, 0));
            } else {
                w->Reply(state, Timestamp());
            }
            return;
        }
        if(timestamp > 0){
            w->Reply(shard, state, Timestamp(timestamp, 0));
        } else {
            w->Reply(shard, state, Timestamp());
        }
    }

}

void
ShardClient::PrepareTimeout(uint64_t tid)
{
    Debug("Shard %lu receive the prepare timeout of %lu", shard, tid);

    if(waiting != NULL){
        Debug("Waiting tid %lu", waiting->GetTid());
    }
    Debug("[shard %i] Prepare timeout", shard);
    Debug("tid: %lu", tid);
    if(waiting != NULL)
        Debug("waiting: %lu", waiting->GetTid());

    if (waiting != NULL && waiting->GetTid() == tid) {
        Promise *w = waiting;
        // waiting = NULL;
        w->Reply(shard, REPLY_TIMEOUT, Timestamp());
    }
}

void
ShardClient::ParallelModeCommitCallback(const uint64_t tid, int state, uint64_t timestamp){
    Debug("Received ParallelModeCommit callback of %lu, state [%d]", tid, state);

    if(waiting != NULL){
        Debug("Waiting tid %lu", waiting->GetTid());
    } else {
        Debug("Waiting tid %lu is NULL", tid);
    }

    if (waiting != NULL && waiting->GetTid() == tid) {
        Debug("set waiting as null for %lu", tid);
        Promise *w = waiting;
        waiting = NULL;
        // w->Reply(state, Timestamp(), true);
        if(timestamp > 0){
            w->Reply(state, Timestamp(timestamp, 0), true);
        } else {
            w->Reply(state, Timestamp(), true);
        }
    }
}

// void
// ShardClient::PrepareCallback(const string &request_str, const string &reply_str)
// {
//     Reply reply;

//     reply.ParseFromString(reply_str);
//     Debug("[shard %i] Received PREPARE callback [%d]", shard, reply.status());

//     if (waiting != NULL) {
//         Promise *w = waiting;
//         waiting = NULL;
//         if (reply.has_timestamp()) {
//             w->Reply(reply.status(), Timestamp(reply.timestamp(), 0));
//         } else {
//             w->Reply(reply.status(), Timestamp());
//         }
//     }
// }

/* Callback from a shard replica on commit operation completion. */
// void
// ShardClient::CommitCallback(const string &request_str, const string &reply_str)
// {
//     // COMMITs always succeed.
//     Reply reply;
//     reply.ParseFromString(reply_str);
//     ASSERT(reply.status() == REPLY_OK);

//     ASSERT(blockingBegin != NULL);
//     blockingBegin->Reply(0);

//     if (waiting != NULL) {
//         Promise *w = waiting;
//         waiting = NULL;
//         w->Reply(reply.status());
//     }
//     Debug("[shard %i] Received COMMIT callback [%d]", shard, reply.status());
// }

// /* Callback from a shard replica on abort operation completion. */
// void
// ShardClient::AbortCallback(const string &request_str, const string &reply_str)
// {
//     // ABORTs always succeed.
//     Reply reply;
//     reply.ParseFromString(reply_str);
//     ASSERT(reply.status() == REPLY_OK);

//     ASSERT(blockingBegin != NULL);
//     blockingBegin->Reply(0);

//     if (waiting != NULL) {
//         Promise *w = waiting;
//         waiting = NULL;
//         w->Reply(reply.status());
//     }
//     Debug("[shard %i] Received ABORT callback [%d]", shard, reply.status());
// }

} // namespace strongstore
