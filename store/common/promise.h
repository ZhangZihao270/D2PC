
#ifndef _PROMISE_H_
#define _PROMISE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "store/common/transaction.h"

#include <condition_variable>
#include <mutex>
#include <vector>

class Promise
{
private:
    bool done;
    int timeout;
    int reply;
    uint64_t tid;
    std::unordered_map<int, int> preparereplies;
    int participantNumber; //participants number
    bool reachFinalDecision = false;
    bool fail = false;
    Timestamp timestamp;
    std::string value;
    std::mutex lock;
    std::condition_variable cv;

    void ReplyInternal(int r);

public:
    Promise();
    Promise(int timeoutMS); // timeout in milliseconds
    Promise(uint64_t id, std::vector<int> participants, int timeoutMS);
    ~Promise();

    // reply to this promise and unblock any waiting threads
    void Reply(int r);
    void Reply(int r, Timestamp t);
    void Reply(int r, std::string v);
    void Reply(int r, Timestamp t, std::string v);
    void Reply(int s, int r, Timestamp t); // reply from a shard
    void Reply(int r, Timestamp t, bool p); // for parallel mode result

    // Return configured timeout
    int GetTimeout();
    uint64_t GetTid();
    void Reset();

    // block on this until response comes back
    int GetReply();
    Timestamp GetTimestamp();
    std::string GetValue();
    int GetPrepareReply();
    bool HasReachFinalDecision(){return reachFinalDecision;}
};

#endif /* _PROMISE_H_ */
