
#include <thread>
#include <memory>

#include "lib/configuration.h"
#include "lib/repltransport.h"
#include "lockserver/client.h"
#include "lockserver/server.h"
#include "replication/ir/replica.h"

int main() {
    ReplTransport transport;
    std::vector<transport::ReplicaAddress> replica_addrs = {
        {"replica", "0"},
        {"replica", "1"},
        {"replica", "2"},
        {"replica", "3"},
        {"replica", "4"}};
    transport::Configuration config(5 /* n */, 2 /* f */, replica_addrs);

    // Clients.
    lockserver::LockClient client_a(&transport, config);
    lockserver::LockClient client_b(&transport, config);
    lockserver::LockClient client_c(&transport, config);
    client_a.lock_async("a");
    client_b.lock_async("b");
    client_c.lock_async("c");

    // Servers.
    std::vector<std::unique_ptr<lockserver::LockServer>> servers;
    std::vector<std::unique_ptr<replication::ir::IRReplica>> replicas;
    for (std::size_t i = 0; i < replica_addrs.size(); ++i) {
        auto server = std::unique_ptr<lockserver::LockServer>(
            new lockserver::LockServer());
        servers.push_back(std::move(server));
        auto replica = std::unique_ptr<replication::ir::IRReplica>(
            new replication::ir::IRReplica(config, i, &transport,
                                           servers[i].get()));
        replicas.push_back(std::move(replica));
    }

    // Launch REPL.
    transport.Run();

    // Remove persisted files.
    for (std::size_t i = 0; i < replica_addrs.size(); ++i) {
        const transport::ReplicaAddress &addr = replica_addrs[i];
        const std::string filename =
            addr.host + ":" + addr.port + "_" + std::to_string(i) + ".bin";
        int success = std::remove(filename.c_str());
        ASSERT(success == 0);
    }
}
