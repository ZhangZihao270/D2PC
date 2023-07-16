
 
#ifndef _TAPIR_CLIENT_H_
#define _TAPIR_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/bufferclient.h"
#include "store/tapirstore/shardclient.h"
#include "store/tapirstore/tapir-proto.pb.h"

#include <thread>

namespace tapirstore {

class Client : public ::Client
{
public:
    Client(const std::string configPath, int nShards,
	   int closestReplica, TrueTime timeserver = TrueTime(0,0));
    virtual ~Client();

    // Overriding functions from ::Client.
    void Begin();
    int Get(const std::string &key, std::string &value);
    // Interface added for Java bindings
    std::string Get(const std::string &key);
    int Put(const std::string &key, const std::string &value);
    bool Commit();
    void Abort();
    std::vector<int> Stats();

private:
    // Unique ID for this client.
    uint64_t client_id;

    // Ongoing transaction ID.
    uint64_t t_id;

    // Number of shards.
    uint64_t nshards;

    // Number of retries for current transaction.
    long retries;

    // List of participants in the ongoing transaction.
    std::set<int> participants;

    // Transport used by IR client proxies.
    UDPTransport transport;
    
    // Thread running the transport event loop.
    std::thread *clientTransport;

    // Buffering client for each shard.
    std::vector<BufferClient *> bclient;

    // TrueTime server.
    TrueTime timeServer;

    // Prepare function
    int Prepare(Timestamp &timestamp);

    // Runs the transport event loop.
    void run_client();
};

} // namespace tapirstore

#endif /* _TAPIR_CLIENT_H_ */
