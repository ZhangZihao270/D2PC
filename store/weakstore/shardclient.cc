

#include "store/weakstore/shardclient.h"

namespace weakstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(string configPath, Transport *transport,
                   uint64_t client_id, int shard, int closestReplica) :
    transport(transport), client_id(client_id), shard(shard)
{ 
    ifstream configStream(configPath);
    if (configStream.fail()) {
        fprintf(stderr, "unable to read configuration file: %s\n",
                configPath.c_str());
    }
    config = new transport::Configuration(configStream);
    transport->Register(this, *config, -1);
    
    timeout = new Timeout(transport, 250, [this]() {
            RequestTimedOut();
        });
    
    if (closestReplica == -1) {
        replica = client_id % config->n;
    } else {
        replica = closestReplica;
    }
    
    waiting = NULL;
}

ShardClient::~ShardClient() 
{ 
    delete config;
    delete timeout;
}

void
ShardClient::Get(uint64_t id, const string &key, Promise *promise)
{
    // Create get request
    GetMessage msg;
    msg.set_clientid(client_id);
    msg.set_key(key);

    ASSERT(waiting == NULL);

    waiting = promise;

    // Send message
    transport->Timer(0, [=]() {
            if (transport->SendMessageToReplica(this, replica, msg)) {                
                if (waiting != NULL) {
                    timeout->SetTimeout(promise->GetTimeout());
                    timeout->Start();
                }
            } else if (waiting != NULL) {
                Promise *w = waiting;
                waiting = NULL;
                w->Reply(REPLY_NETWORK_FAILURE);
            }
        });
}

void
ShardClient::Put(uint64_t id,
              const string &key,
              const string &value,
              Promise *promise)
{
    Debug("[shard %d] Sending PUT [%s %s]", shard, key.c_str(), value.c_str());

    // Creating put request
    PutMessage msg;
    msg.set_clientid(client_id);
    msg.set_key(key);
    msg.set_value(value);

    ASSERT(waiting == NULL);

    waiting = promise;
    // clear the reply counter
    totalReplies = 0;

    // Send messages
    transport->Timer(0, [=]() {
            // always send to leader for now
            if (transport->SendMessageToAll(this, msg)) {
                // set the timeout
                if (waiting != NULL) {
                    timeout->SetTimeout(waiting->GetTimeout());
                    timeout->Start();
                }
            } else if (waiting != NULL) {
                Promise *w = waiting;
                waiting = NULL;
                w->Reply(REPLY_NETWORK_FAILURE);
            }
        });

}

// Callbacks that happen in the transport thread
void
ShardClient::RequestTimedOut()
{
    Debug("[shard %d] Timeout", shard);
    
    timeout->Stop();

    if (waiting != NULL) {
        Promise *w = waiting;
        waiting = NULL;
        w->Reply(REPLY_TIMEOUT);
    }
}

void
ShardClient::ReceiveMessage(const TransportAddress &remote,
                         const string &type,
                         const string &data)
{
    GetReplyMessage getReply;
    PutReplyMessage putReply;
  
    Debug("Received reply type: %s", type.c_str());

    if (type == getReply.GetTypeName()) {
        getReply.ParseFromString(data);
        if (waiting != NULL) {
            timeout->Stop();
            Promise *w = waiting;
            waiting = NULL;
            w->Reply(getReply.status(),getReply.value());
        }
    } else if (type == putReply.GetTypeName()) {
        totalReplies++;
        if (totalReplies >= config->n) {
            if (waiting != NULL) {
                timeout->Stop();
                Promise *w = waiting;
                waiting = NULL;
                w->Reply(REPLY_OK);
            }
        }
    } else {
        NOT_REACHABLE();
    }
}

} // namespace weakstore
