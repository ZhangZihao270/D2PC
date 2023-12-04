

#ifndef _WEAK_CLIENT_H_
#define _WEAK_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/udptransport.h"
#include "replication/common/client.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/bufferclient.h"
#include "store/common/truetime.h"
#include "store/weakstore/shardclient.h"
#include "store/weakstore/weak-proto.pb.h"

#include <string>
#include <thread>
#include <set>

namespace weakstore {

class Client : public ::Client
{
public:
    Client(std::string configPath, int nshards, int closestReplica);
    ~Client();

    // Overriding methods from ::Client
    void Begin() {};
    int Get(const std::string &key, std::string &value);
    int MultiGet(const std::vector<std::string> &key, std::string &value);
    int Put(const std::string &key, const std::string &value);
    bool Commit() { return true; };
    void Abort() {};
    std::vector<int> Stats();

private:
    /* Private helper functions. */
    void run_client(); // Runs the transport event loop.

    // Unique ID for this client.
    uint64_t client_id;

    // Number of shards in this deployment
    uint64_t nshards;

    // Transport used by shard clients.
    UDPTransport transport;
    
    // Thread running the transport event loop.
    std::thread *clientTransport;

    // Client for each shard.
    std::vector<ShardClient *> bclient;
};

} // namespace weakstore

#endif /* _WEAK_CLIENT_H_ */
