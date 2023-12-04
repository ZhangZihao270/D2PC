

#include "store/common/promise.h"

using namespace std;

Promise::Promise() 
{ 
    done = false;
    reply = 0;
    timeout = 1000;
}

Promise::Promise(int timeoutMS) 
{ 
    done = false;
    reply = 0;
    timeout = timeoutMS;
}

Promise::Promise(uint64_t id, std::vector<int> participants, int timeoutMS){
    tid = id;
    done = false;
    reply = 0;
    participantNumber = participants.size();
    timeout = timeoutMS;
    timestamp = Timestamp();
}

Promise::~Promise() { }

// Get configured timeout, return after this period
int
Promise::GetTimeout()
{
    return timeout;
}

uint64_t
Promise::GetTid()
{
    return tid;
}

// Functions for replying to the promise
void
Promise::ReplyInternal(int r)
{
    done = true;
    reply = r;
    cv.notify_all();
}

void
Promise::Reply(int r)
{
    lock_guard<mutex> l(lock);
    ReplyInternal(r);
}

void
Promise::Reply(int r, Timestamp t)
{
    lock_guard<mutex> l(lock);
    reachFinalDecision = true;
    timestamp = t;
    ReplyInternal(r);
}

void
Promise::Reply(int r, string v)
{
    lock_guard<mutex> l(lock);
    value = v;
    ReplyInternal(r);
}

void
Promise::Reply(int r, Timestamp t, string v)
{
    lock_guard<mutex> l(lock);
    value = v;
    timestamp = t;
    ReplyInternal(r);
}

void 
Promise::Reply(int s, int r, Timestamp t){
    if(r != REPLY_OK){
        fail = true;
        Reply(r);
    }
    preparereplies[s] = r;
    if(timestamp < t)
        timestamp = t;
    if(preparereplies.size() == participantNumber){
        lock_guard<mutex> l(lock);
        done = true;
        cv.notify_all();
    }
}

void
Promise::Reply(int r, Timestamp t, bool p){
    Debug("Reach final decision");
    lock_guard<mutex> l(lock);
    reachFinalDecision = true;
    timestamp = t;
    ReplyInternal(r);
}

// void 
// Promise::Reply(int r, std::vector<Timestamp> t, std::vector<std::string> v){
//     lock_guard<mutex> l(lock);
//     value = v;
//     timestamp = t;
//     ReplyInternal(r);
// }

// Functions for getting a reply from the promise
int
Promise::GetReply()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return reply;
}

Timestamp
Promise::GetTimestamp()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return timestamp;
}
    
string
Promise::GetValue()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return value;
}

int
Promise::GetPrepareReply(){
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    if(fail){
        return reply;
    }
    if(reachFinalDecision){
        Debug("Txn %lu reach final decision", tid);
        return reply;
    } else {
        for(auto p : preparereplies){
            if(p.second != REPLY_OK){
                return p.second;
            }
        }
        Debug("Txn %lu reach normal commit", tid);
        return REPLY_OK;
    }
}

void Promise::Reset(){
    done = false;
}
