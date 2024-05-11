

#include "store/strongstore/client.h"
#include <nlohmann/json.hpp>

using namespace std;

using json = nlohmann::json;

namespace strongstore {

Client::Client(Mode mode, TpcMode tpcMode, string configPath, int nShards,
                int closestReplica, int n, TrueTime timeServer, bool tpcc)
    : transport(0.0, 0.0, 0), mode(mode), tpcMode(tpcMode), close_replica(closestReplica), nreplicas(n), timeServer(timeServer), tpcc(tpcc)
{
    // Initialize all state here;
    client_id = 0;
    while (client_id == 0) {
        random_device rd;
        mt19937_64 gen(rd());
        uniform_int_distribution<uint64_t> dis;
        client_id = dis(gen);
    }
    t_id = (client_id/10000)*10000;

    nshards = nShards;
    bclient.reserve(nshards);

    Debug("Initializing SpanStore client with id [%lu]", client_id);

    /* Start a client for each shard. */
    for (int i = 0; i < nShards; i++) {
        string shardConfigPath = configPath + to_string(i) + ".config";
        ShardClient *shardclient = new ShardClient(mode, tpcMode, shardConfigPath,
            &transport, client_id, i, closestReplica);
        // ShardClient *shardclient = new ShardClient(mode, tpcMode, shardConfigPath,
        //     &transport, client_id, i, i);
        bclient[i] = new BufferClient(shardclient);
    }


    /* Run the transport in a new thread. */
    clientTransport = new thread(&Client::run_client, this);

    Debug("SpanStore client [%lu] created!", client_id);
}

Client::~Client()
{
    transport.Stop();

    // delete tss;
    for (auto b : bclient) {
        delete b;
    }

    clientTransport->join();
}

/* Runs the transport event loop. */
void
Client::run_client()
{
    transport.Run();
}


/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 *
 * Return a TID for the transaction.
 */
void
Client::Begin()
{
    Debug("BEGIN Transaction");
    t_id++;
    Debug("txn id: %lu", t_id);
    participants.clear();
    commit_sleep = -1;
    for (int i = 0; i < nshards; i++) {
        bclient[i]->Begin(t_id);
    }
    v.clear();
}

/* Returns the value corresponding to the supplied key. */
int
Client::Get(const string &key, string &value)
{
    // Debug("client read");
    // Contact the appropriate shard to get the value.
    int i;
    if(tpcc){
        // Debug("tpcc, key: %s", key.c_str());
        i = key_to_shard(key, 0);
    } else {
        i = key_to_shard(key, nshards);
    }

    // If needed, add this shard to set of participants and send BEGIN.
    // if (participants.find(i) == participants.end()) {
    //     participants.insert(i);
    // }

    if(std::find(participants.begin(), participants.end(), i) == participants.end()){
        participants.push_back(i);
    }

    // Send the GET operation to appropriate shard.
    Promise promise(GET_TIMEOUT);

    bclient[i]->Get(key, &promise);
    value = promise.GetValue();
    
    Debug("Get %s of txn %lu, state %d", key.c_str(), promise.GetTid(), promise.GetReply());

    return promise.GetReply();
}

/* Returns the value corresponding to the supplied key. */
int 
Client::MultiGet(const std::vector<std::string> &keys, std::string &value)
{
    vector<vector<string>> key_shards(nshards, vector<string>());
    // Contact the appropriate shard to get the value.
    for(string k : keys){
        int i;
        if(tpcc){
            i = key_to_shard(k, 0);
        } else {
            i = key_to_shard(k, nshards);
        }
        // int i = key_to_shard(k, nshards);
        key_shards[i].push_back(k);

        if(std::find(participants.begin(), participants.end(), i) == participants.end()){
            participants.push_back(i);
        }
    }

    // If needed, add this shard to set of participants and send BEGIN.
    // if (participants.find(i) == participants.end()) {
    //     participants.insert(i);
    // }

    Promise promise(GET_TIMEOUT);

    for(int i = 0; i < nshards; i++){
    // Send the GET operation to appropriate shard.
        if(!key_shards[i].empty()){
            promise.Reset();
            bclient[i]->MultiGet(key_shards[i], &promise);
            Debug("[Shardd %d] read reply status %d", i, promise.GetReply());
            if(promise.GetReply() == 1)
                return promise.GetReply();

            value = promise.GetValue();
        
            // Debug("Get %s of txn %lu, state %d", key.c_str(), promise.GetTid(), promise.GetReply());
        }
    }

    return promise.GetReply();
}

/* Sets the value corresponding to the supplied key. */
int
Client::Put(const string &key, const string &value)
{
    // Contact the appropriate shard to set the value.
    int i;
    if(tpcc){
        i = key_to_shard(key, 0);
    } else {
        i = key_to_shard(key, nshards);
    }

    Debug("put %s to shard %d", key.c_str(), i);

    // If needed, add this shard to set of participants and send BEGIN.
    // if (participants.find(i) == participants.end()) {
    //     participants.insert(i);
    // }

    if(std::find(participants.begin(), participants.end(), i) == participants.end()){
        participants.push_back(i);
    }

    Promise promise(PUT_TIMEOUT);

    // Buffering, so no need to wait.
    bclient[i]->Put(key, value, &promise);
    return promise.GetReply();
}

int
Client::Prepare(uint64_t &ts)
{
    // int status;
    int status;
    
    Debug("PREPARE Transaction %lu", t_id);

    int primaryShard = -1;
    bool primaryIsNotParticipant = false;

    if(tpcMode == TpcMode::MODE_SLOW || tpcMode == TpcMode::MODE_RC){
        // choose one of the participant as the primary shard
        primaryShard = participants[t_id % participants.size()];
    } else if (tpcMode == TpcMode::MODE_PARALLEL){
        // choose the leader on the same region as the primary shard
        for(auto p : participants){
            if(p % nreplicas == close_replica){
                primaryShard = p;
            }
        }
        
        if(primaryShard == -1){
            primaryIsNotParticipant = true;
            for(int i = 0; i < nshards; i++){
                if(i % nreplicas == close_replica){
                    primaryShard = i;
                }
            }
        }
    }
    Debug("%lu primary shard %d", t_id, primaryShard);

    Promise * promise = new Promise(t_id, participants, PREPARE_TIMEOUT);

    if(tpcMode == TpcMode::MODE_SLOW || tpcMode == TpcMode::MODE_RC){
        bclient[primaryShard]->SetParticipants(participants);
        if(participants.size() == 0)
        {
            return 0;
        }
        // gather the read and write set on all shards, and send to the primary shard leader.
        for(auto p : participants){
            for(auto k : bclient[p]->GetReadSet()){
                bclient[primaryShard]->AddReadSet(k.first, k.second);
            }
            for(auto k : bclient[p]->GetWriteSet()){
                bclient[primaryShard]->AddWriteSet(k.first, k.second);
            }
        }
        bclient[primaryShard]->SetPrimaryShard(primaryShard);
        bclient[primaryShard]->Prepare(Timestamp(), promise);
    }
    // fast mode and parallel mode
    if(tpcMode == TpcMode::MODE_FAST || tpcMode == TpcMode::MODE_PARALLEL || 
        tpcMode == TpcMode::MODE_CAROUSEL){
            if(participants.size() == 0){
                return 0;
            }
        for (auto p : participants) {
            Debug("Sending prepare to shard [%d]", p);
            bclient[p]->SetParticipants(participants);
            bclient[p]->SetPrimaryShard(primaryShard);
            bclient[p]->Prepare(Timestamp(), promise);

            if(p == primaryShard){
                bclient[primaryShard]->ParallelModeCommit(promise);
            }
            // else {
            //     bclient[p]->Prepare(Timestamp(), promise);
            // }
        }
    }


    if(primaryIsNotParticipant && tpcMode == TpcMode::MODE_PARALLEL){
        bclient[primaryShard]->SetParticipants(participants);
        bclient[primaryShard]->ParallelModeCommit(promise);
    }

    // // 2. Wait for reply from all shards. (abort on timeout)
    Debug("Waiting for PREPARE replies");
    status = promise->GetPrepareReply();
    ts = promise->GetTimestamp().getTimestamp();
    Debug("Client receive the result of %lu, state %d", t_id, status);
    delete promise;

    return status;
}

/* Attempts to commit the ongoing transaction. */
bool
Client::Commit()
{
    // Implementing 2 Phase Commit
    uint64_t ts = 0;
    int status;
    int retry_num = 0;

    for (; retry_num < COMMIT_RETRIES; retry_num++) {
        status = Prepare(ts);
        Debug("Status: %d", status);
        if (status == REPLY_OK || status == REPLY_ABORT || status == REPLY_FAIL) {
            Debug("%d, %d, %d", REPLY_OK, REPLY_ABORT, REPLY_FAIL);
            break;
        }
    }
    Debug("retry times: %d", retry_num);

    if (status == REPLY_OK) {
        // For Spanner like systems, calculate timestamp.
        if (mode == MODE_SPAN_OCC || mode == MODE_SPAN_LOCK) {
            uint64_t now, err;
            struct timeval t1, t2;

            gettimeofday(&t1, NULL);
            timeServer.GetTimeAndError(now, err);

            if (now > ts) {
                ts = now;
            } else {
                uint64_t diff = ((ts >> 32) - (now >> 32))*1000000 +
                        ((ts & 0xffffffff) - (now & 0xffffffff));
                err += diff;
            }

            commit_sleep = (int)err;

            // how good are we at waking up on time?
            Debug("Commit wait sleep: %lu", err);
            if (err > 1000000)
                Warning("Sleeping for too long! %lu; now,ts: %lu,%lu", err, now, ts);
            if (err > 150) {
                usleep(err-150);
            }
            // fine grained busy-wait
            while (1) {
                gettimeofday(&t2, NULL);
                if ((t2.tv_sec-t1.tv_sec)*1000000 +
                    (t2.tv_usec-t1.tv_usec) > (int64_t)err) {
                    break;
                }
            }
        }

        // Send commits
        Debug("COMMIT Transaction %lu at [%lu]", t_id, ts);

        for (auto p : participants) {
            // Debug("Sending commit to shard [%d]", p);
            bclient[p]->Commit(ts);
        }
        v.push_back(retry_num);
        return true;
    }

    // 4. If not, send abort to all shards.
    Abort();
    v.push_back(retry_num);
    Debug("retry num: %d", v[0]);
    return false;
}

/* Aborts the ongoing transaction. */
void
Client::Abort()
{
    Debug("ABORT Transaction %lu", t_id);
    for (auto p : participants) {
        bclient[p]->Abort();
    }
}

/* Return statistics of most recent transaction. */
vector<int>
Client::Stats()
{
    // vector<int> v;
    return v;
}

} // namespace strongstore
