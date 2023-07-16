

#ifndef _IR_LOCK_CLIENT_H_
#define _IR_LOCK_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/ir/client.h"
#include "store/common/promise.h"
#include "lockserver/locks-proto.pb.h"

#include <map>
#include <set>
#include <string>
#include <thread>
#include <random>

namespace lockserver {

class LockClient
{
public:
    LockClient(Transport* transport, const transport::Configuration &config);
    ~LockClient();

    // Synchronously lock and unlock. Calling lock (or unlock) will block until
    // the lock (or unlock) request is fully processed.
    bool lock(const std::string &key);
    void unlock(const std::string &key);

    // Asynchronously lock and unlock. Calling lock_async or unlock_async will
    // not block. Calling lock_wait (or unlock_wait) will block for the
    // previous invocation of lock_async (or unlock_async) to complete.
    //
    // All async calls must be followed by a corresponding wait call. It is an
    // error to issue multiple async requests without waiting. It is also
    // erroneous to wait for a request which was never issued.
    void lock_async(const std::string &key);
    bool lock_wait();
    void unlock_async(const std::string &key);
    void unlock_wait();

private:
    /* Unique ID for this client. */
    uint64_t client_id;

    /* Transport layer and thread. */
    Transport *transport;

    /* Function to run the transport thread. */
    void run_client();

    /* Decide function for a lock server. */
    string Decide(const std::map<string, std::size_t> &results);

    /* IR client proxy. */
    replication::ir::IRClient *client;

    /* Promise to wait for pending operation. */
    Promise *waiting = nullptr;

    /* Callbacks for hearing back for an operation. */
    void LockCallback(const std::string &, const std::string &);
    void UnlockCallback(const std::string &, const std::string &);
    void ErrorCallback(const std::string &, replication::ErrorCode);
};

} // namespace lockserver

#endif /* _IR_LOCK_CLIENT_H_ */
