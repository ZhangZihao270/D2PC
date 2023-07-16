

#include "store/weakstore/client.h"

namespace weakstore {

using namespace std;
using namespace proto;

Client::Client(string configPath, int nShards, int closestReplica)
    : transport(0.0, 0.0, 0)
{
    // Initialize all state here;
    client_id = 0;
    while (client_id == 0) {
        random_device rd;
        mt19937_64 gen(rd());
        uniform_int_distribution<uint64_t> dis;
        client_id = dis(gen);
    }
    
    nshards = nShards;
    bclient.reserve(nShards);

    Debug("Initializing WeakStore client with id [%lu]", client_id);

    /* Start a client for each shard. */
    for (int i = 0; i < nShards; i++) {
        string shardConfigPath = configPath + to_string(i) + ".config";
        bclient[i] = new ShardClient(shardConfigPath, &transport,
            client_id, i, closestReplica);
    }

    /* Run the transport in a new thread. */
    clientTransport = new thread(&Client::run_client, this);

    Debug("WeakStore client [%lu] created!", client_id);
}

Client::~Client()
{
    transport.Stop();
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

/* Returns the value corresponding to the supplied key. */
int
Client::Get(const string &key, string &value)
{
    Debug("GET Operation [%s]", key.c_str());

    // Contact the appropriate shard to get the value.
    int i = key_to_shard(key, nshards);

    // Send the GET operation to appropriate shard.
    Promise promise;

    bclient[i]->Get(client_id, key, &promise);
    value = promise.GetValue();
    return promise.GetReply();
}

/* Sets the value corresponding to the supplied key. */
int
Client::Put(const string &key,
            const string &value)
{
    Debug("PUT Operation [%s]", key.c_str());

    // Contact the appropriate shard to set the value.
    int i = key_to_shard(key, nshards);

       // Send the GET operation to appropriate shard.
    Promise promise;

    bclient[i]->Put(client_id, key, value, &promise);
    return promise.GetReply();
}

vector<int>
Client::Stats()
{
    vector<int> v;
    return v;
}

} // namespace weakstore
