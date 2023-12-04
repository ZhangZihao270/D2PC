
 
#ifndef _STRONG_CLIENT_H_
#define _STRONG_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
// #include "replication/vr/client.h"
#include "replication/lr/client.h"
#include "store/common/frontend/client.h"
#include "store/common/truetime.h"
#include "store/strongstore/strong-proto.pb.h"
#include "store/common/frontend/bufferclient.h"
#include "store/strongstore/shardclient.h"

#include <set>
#include <thread>

namespace strongstore {

class Client : public ::Client
{
public:
    Client(Mode mode, TpcMode tpcMode, string configPath, int nshards,
            int closestReplica, int n, TrueTime timeServer);
    ~Client();

    // Overriding functions from ::Client
    void Begin();
    int Get(const string &key, string &value);
    int MultiGet(const std::vector<std::string> &keys, std::string &value);
    int Put(const string &key, const string &value);
    bool Commit();
    void Abort();
    std::vector<int> Stats();

    // void commitCallback(int &s);



private:
    /* Private helper functions. */
    void run_client(); // Runs the transport event loop.

    // timestamp server call back
    // void tssCallback(const string &request, const string &reply);

    // void run_commit_client();
    // void run_parallel_mode_commit_client();

    // local Prepare function
    int Prepare(uint64_t &ts);

    // Unique ID for this client.
    uint64_t client_id;

    // Ongoing transaction ID.
    uint64_t t_id;

    // Number of shards in SpanStore.
    long nshards;

    // Number of replicas of each shard.
    int nreplicas;

    // The replica id in the same region with this client.
    int close_replica;

    // List of participants in the ongoing transaction.
    std::vector<int> participants;

    // Transport used by paxos client proxies.
    UDPTransport transport;
    
    // Thread running the transport event loop.
    std::thread *clientTransport;

    // Buffering client for each shard.
    std::vector<BufferClient *> bclient;

    // CommitClient *commitClient;
    // std::thread *commitClientThread;

    // CommitClient *parallelModeCommitClient;
    // std::thread *parallelModeCommitClientThread;

    // Mode in which spanstore runs.
    Mode mode;

    TpcMode tpcMode;

    // Timestamp server shard.
    // replication::vr::VRClient *tss; 

    // TrueTime server.
    TrueTime timeServer;

    // Synchronization variables.
    std::condition_variable cv;
    std::mutex cv_m;
    string replica_reply;

    // Use for transaction commit callback
    // std::condition_variable commit_cv;
    // std::mutex commit_cv_m;
    // int status;

    // Time spend sleeping for commit.
    int commit_sleep;

    vector<int> v;
};

} // namespace strongstore

#endif /* _STRONG_CLIENT_H_ */
