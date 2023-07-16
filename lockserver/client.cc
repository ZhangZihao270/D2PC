

#include "lockserver/client.h"

namespace lockserver {

using namespace std;
using namespace proto;

LockClient::LockClient(Transport *transport,
                       const transport::Configuration &config)
    : transport(transport) {
    client_id = 0;
    while (client_id == 0) {
        random_device rd;
        mt19937_64 gen(rd());
        uniform_int_distribution<uint64_t> dis;
        client_id = dis(gen);
    }

    client = new replication::ir::IRClient(config, transport, client_id);
}

LockClient::~LockClient() { }

void
LockClient::lock_async(const std::string &key) {
    ASSERT(waiting == nullptr);
    Debug("Sending LOCK");

    string request_str;
    Request request;
    request.set_clientid(client_id);
    request.set_key(key);
    request.set_type(true);
    request.SerializeToString(&request_str);

    waiting = new Promise(1000);
    transport->Timer(0, [=]() {
            client->InvokeConsensus(request_str,
                bind(&LockClient::Decide,
                    this,
                    placeholders::_1),
                bind(&LockClient::LockCallback,
                    this,
                    placeholders::_1,
                    placeholders::_2),
                bind(&LockClient::ErrorCallback,
                    this,
                    placeholders::_1,
                    placeholders::_2));
            });
}

bool
LockClient::lock_wait() {
    ASSERT(waiting != nullptr);

    int status = waiting->GetReply();
    delete waiting;
    waiting = nullptr;

    if (status == 0) {
        return true;
    } else if (status == -1) {
        Debug("Lock held by someone else.");
    }
    return false;
}

void
LockClient::unlock_async(const std::string &key) {
    ASSERT(waiting == nullptr);
    Debug("Sending UNLOCK");

    string request_str;
    Request request;
    request.set_clientid(client_id);
    request.set_key(key);
    request.set_type(false);
    request.SerializeToString(&request_str);

    waiting = new Promise(1000);
    transport->Timer(0, [=]() {
            client->InvokeInconsistent(request_str,
                bind(&LockClient::UnlockCallback,
                    this,
                    placeholders::_1,
                    placeholders::_2));
            });
}

void
LockClient::unlock_wait() {
    waiting->GetReply();
    delete waiting;
    waiting = nullptr;
}

bool
LockClient::lock(const string &key)
{
    lock_async(key);
    return lock_wait();
}

void
LockClient::unlock(const string &key)
{
    unlock_async(key);
    return unlock_wait();
}

string
LockClient::Decide(const map<string, std::size_t> &results)
{
    // If a majority say lock, we say lock.
    int success_count = 0;
    string key;
    for (const auto& string_and_count : results) {
        const string& s = string_and_count.first;
        const std::size_t count = string_and_count.second;

        Reply reply;
        reply.ParseFromString(s);
        key = reply.key();

        if (reply.status() == 0) {
            success_count += count;
        }
    }

    string final_reply_str;
    Reply final_reply;
    final_reply.set_key(key);
    if (success_count >= 2) {
        final_reply.set_status(0);
    } else {
        final_reply.set_status(-1);
    }
    final_reply.SerializeToString(&final_reply_str);
    return final_reply_str;
}

void
LockClient::LockCallback(const std::string &request_str, const std::string &reply_str)
{
    Debug("Lock Callback: %s %s", request_str.c_str(), reply_str.c_str());
    Reply reply;
    reply.ParseFromString(reply_str);

    Promise *w = waiting;
    waiting = NULL;
    w->Reply(reply.status());
}

void
LockClient::UnlockCallback(const std::string &request_str, const std::string &reply_str)
{
    Debug("Lock Callback: %s %s", request_str.c_str(), reply_str.c_str());

    Promise *w = waiting;
    waiting = NULL;
    w->Reply(0);
}

void
LockClient::ErrorCallback(const std::string &request_str,
                          replication::ErrorCode err)
{
    Debug("Error Callback: %s %s", request_str.c_str(),
          replication::ErrorCodeToString(err).c_str());
    Promise *w = waiting;
    waiting = NULL;
    w->Reply(-3);  // Invalid command.
}

} // namespace lockserver
