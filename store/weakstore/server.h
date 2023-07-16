

#ifndef _WEAK_SERVER_H_
#define _WEAK_SERVER_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/udptransport.h"
#include "lib/configuration.h"
#include "store/weakstore/store.h"
#include "store/weakstore/weak-proto.pb.h"

namespace weakstore {

class Server : TransportReceiver
{
private:
    // Underlying single node transactional key-value store.
    Store *store;

    // Configuration of replicas.
    transport::Configuration configuration;

    // Index of 'this' replica, and handle to transport layer.
    Transport *transport;

public:
    Server(const transport::Configuration &configuration, int myIdx,
           Transport *transport, Store *store);
    ~Server();

    void ReceiveMessage(const TransportAddress &remote,
                        const std::string &type, const std::string &data);

    void HandleMessage(const TransportAddress &remote,
                        const std::string &type, const std::string &data);
    void HandleGet(const TransportAddress &remote,
                   const proto::GetMessage &msg);
    void HandlePut(const TransportAddress &remote,
                   const proto::PutMessage &msg);

    void Load(const std::string &key, const std::string &value);

};


} // namespace weakstore

#endif /* _WEAK_SERVER_H_ */
