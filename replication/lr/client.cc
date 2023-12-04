#include "replication/lr/client.h"
#include "lib/assert.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/lr/lr-proto.pb.h"

#include <math.h>

namespace replication{
namespace lr{
using namespace std;

LRClient::LRClient(const transport::Configuration &config,
                   Transport *transport, TpcMode tpcMode, uint64_t clientid)
    : config(config), transport(transport), clientid(clientid), tpcMode(tpcMode)
{
    transport->Register(this, config, -1);
}

LRClient::~LRClient(){
    for (auto kv : pendingReqs) {
	    delete kv.second;
    }
    for (auto kv : pendingParallelCommits) {
        delete kv.second;
    }
}

void 
LRClient::ReceiveMessage(const TransportAddress &remote,
                         const string &type,
                         const string &data)
{
    proto::PrepareTransactionReply prepareTxnReply;
    proto::GetDataReply getDataReply;
    proto::ParallelModeCommitReply parallelModeCommitReply;

    if(type == prepareTxnReply.GetTypeName()){
        Debug("aaa111");
        prepareTxnReply.ParseFromString(data);
        HandlePrepareTxnReply(remote, prepareTxnReply);
    } else if (type == getDataReply.GetTypeName()){
        Debug("bbb222");
        getDataReply.ParseFromString(data);
        HandleGetDataReply(remote, getDataReply); 
    } else if (type == parallelModeCommitReply.GetTypeName()){
        Debug("ccc333");
        parallelModeCommitReply.ParseFromString(data);
        HandleParallelModeCommitReply(remote, parallelModeCommitReply);
    }
    else {

    }
}

// fast mode
// invokeprepare will send the sub transaction to the shard leader
void 
LRClient::InvokePrepare(const Transaction &sub_txn,
                        uint64_t leader,
                        continuation_t continuation,
                        error_continuation_t error_continuation){
    uint64_t tid = sub_txn.getID();
    Debug("LRClient prepare txn: %lu", tid);
    auto timer = std::unique_ptr<Timeout>(new Timeout(
        transport, 500, [this, tid]() { PrepareTimeoutCallback(tid); }));
    PendingPrepare *req = 
    new PendingPrepare(tid, leader, std::move(timer), 
        sub_txn, continuation, error_continuation);

    pendingReqs[tid] = req;

    SendPrepare(leader, req);
}

void
LRClient::SendPrepare(uint64_t leader, const PendingPrepare *req){
    Debug("LRClient send prepare of txn %lu to replica %lu", req->txn.getID(), leader);
    proto::PrepareTransaction reqMsg;
    reqMsg.set_tid(req->txn.getID());
    for(const std::pair<std::string, std::string> &wp : req->txn.getWriteSet()){
        proto::WriteSet *wsetEntry = reqMsg.add_wset();

        wsetEntry->set_key(wp.first);
        wsetEntry->set_value(wp.second);
    }

    Debug("11111");

    for(const std::pair<std::string, Timestamp> &rp : req->txn.getReadSet()){
        proto::ReadSet *rsetEntry = reqMsg.add_rset();

        rsetEntry->set_key(rp.first);
        rsetEntry->set_replica(rp.second.getID());
        rsetEntry->set_readts(rp.second.getTimestamp());
    }

    Debug("22222");

    reqMsg.set_primaryshard(req->txn.getPrimaryShard());
    for(auto p : req->txn.getParticipants()){
        reqMsg.add_participants(p);
    }

    Debug("33333");

    switch(tpcMode) {
    case TpcMode::MODE_SLOW:
        reqMsg.set_mode(proto::CommitMode::MODE_SLOW);
        break;

    case TpcMode::MODE_FAST:
        reqMsg.set_mode(proto::CommitMode::MODE_FAST);
        break;

    case TpcMode::MODE_PARALLEL:
        reqMsg.set_mode(proto::CommitMode::MODE_PARALLEL);
        break;
    
    case TpcMode::MODE_CAROUSEL:
        reqMsg.set_mode(proto::CommitMode::MODE_CAROUSEL);
        break;
    }
    Debug("44444");
    // in Carousel and RC mode, the prepare need to send to all replicas
    if(tpcMode == TpcMode::MODE_CAROUSEL || tpcMode == TpcMode::MODE_RC){
        if(transport->SendMessageToAll(this, reqMsg)){
            req->timer->Reset();
        } else {
            Warning("Could not send Prepare request to all replicas");
            pendingReqs.erase(req->txn.getID());
            delete req;
        }
        return;
    }
    Debug("55555");
    // TODO
    // how to known the leader id
    if(transport->SendMessageToReplica(this, leader, reqMsg)){
        req->timer->Reset();
    } else {
        Warning("Could not send Prepare request to shard leader");
        pendingReqs.erase(req->txn.getID());
        delete req;
    }
    Debug("66666");
}

void
LRClient::PrepareTimeoutCallback(const uint64_t tid){
    auto it = pendingReqs.find(tid);
    if (it == pendingReqs.end()) {
        Debug("Received timeout of %lu when no request was pending", tid);
        return;
    }

    PendingPrepare *req = static_cast<PendingPrepare *>(it->second);
    ASSERT(req != NULL);

    Warning("Prepare of one shard timed out [%lu]", tid);
    // delete timer event
    req->timer->Stop();
    // remove from pending list
    pendingReqs.erase(it);
    // invoke application callback
    if (req->error_continuation != nullptr) {
        req->error_continuation(tid, ErrorCode::TIMEOUT);
    }
    delete req;
}

void 
LRClient::HandlePrepareTxnReply(const TransportAddress &remote,
                                const PrepareTransactionReply &msg){
    Debug("Receive prepare reply of txn: %lu on shard: %lu from replica %lu", msg.tid(), msg.shard(), msg.replica());
    uint64_t tid = msg.tid();
    auto it = pendingReqs.find(tid);
    if(it == pendingReqs.end()){
        Debug("Received the reply of a unknown transaction");
        return;
    }

    PendingPrepare *req = dynamic_cast<PendingPrepare *>(it->second);
    ASSERT(req != NULL);

    // Debug("Prapre state: %d", msg.state());

    // In Carousel mode, client may receive replies from both leader and replicas
    // if replies from replicas form a super quorum, it enters fast path
    // if receive replicate success reply from leader, it enters slow path
    if(tpcMode == TpcMode::MODE_CAROUSEL || tpcMode == TpcMode::MODE_RC){
        Debug("Shard %d receive prepare reply from replica %lu", msg.shard(), msg.replica());

        req->replicaReplies[msg.replica()] = msg.state();
        Debug("Shard %d receive reply count %d", msg.shard(), req->replicaReplies.size());
        if(msg.timestamp() > req->timestamp)
            req->timestamp = msg.timestamp();
        
        if(tpcMode == TpcMode::MODE_CAROUSEL && !CarouselCheck(req)){
            if(msg.reachfaulttolerance() == 1){
                Debug("Receive prepare reply from leader %lu", req->leader);
                req->state = msg.state();
                req->reachFaultTolerance = true;
                req->timestamp = msg.timestamp();
            }
            return;
        }

        if(tpcMode == TpcMode::MODE_RC && !RCCheck(req)){
            return;
        }
    } else {
        req->state = msg.state();
        req->timestamp = msg.timestamp();
    }

    if(!req->continuationInvoked){
        // TODO
        // if(msg.state() == LogEntryState::LOG_STATE_PREPAREOK){
        if(req->state == LogEntryState::LOG_STATE_PREPAREOK){
            req->continuation(tid, msg.shard(), 0, req->timestamp);
        } else {
            req->continuation(tid, msg.shard(), 1, req->timestamp);
        }
        req->continuationInvoked = true;
    }
    // receive preparetxn reply means that the transaction log has been replicated to a majority,
    // it reaches consensus
    req->timer->Stop();
    pendingReqs.erase(it);
    delete req;
}

// TODO timeout
void
LRClient::InvokeParallelCommit(
    uint64_t tid,
    std::vector<uint64_t> participants,
    uint64_t replica_id,
    parallelcommit_continuation_t continuation,
    error_continuation_t error_continuation
){
    Debug("LRClient parallel commit txn: %lu", tid);
    auto timer = std::unique_ptr<Timeout>(new Timeout(
        transport, 1000,
        [this, tid]() { ParallelModeCommit(tid); }));
    Debug("aaaaa");
    PendingParallelCommitRequest *req =
    new PendingParallelCommitRequest(tid, participants, 
                                    continuation, error_continuation, 
                                    std::move(timer));
    Debug("bbbbb");
    proto::ParallelModeCommit reqMsg;
    reqMsg.set_tid(tid);
    for(auto p : participants){
        reqMsg.add_participants(p);
    }
    Debug("ccccc");
    if(transport->SendMessageToReplica(this, replica_id, reqMsg)){
        req->timer->Start();
        pendingParallelCommits[tid] = req;
    } else {
        Warning("Could not send ParallelCommit request to replica");
        delete req;
    }
    Debug("ddddd");
}

// TODO, 能够直接放弃吗？
void
LRClient::ParallelModeCommit(const uint64_t tid)
{
    auto it = pendingParallelCommits.find(tid);
    if (it == pendingParallelCommits.end()) {
        Debug("Received parellel commit timeout of %lu when no request was pending", tid);
        return;
    }

    PendingParallelCommitRequest *req = static_cast<PendingParallelCommitRequest *>(it->second);
    ASSERT(req != NULL);

    Warning("Parallel commit request timed out [%lu]", tid);
    // delete timer event
    req->timer->Stop();
    // remove from pending list
    pendingParallelCommits.erase(it);
    // invoke application callback
    if (req->error_continuation) {
        req->error_continuation(tid, ErrorCode::TIMEOUT);
    }
    delete req;
}

void 
LRClient::HandleParallelModeCommitReply(const TransportAddress &remote,
                        const ParallelModeCommitReply &msg){
    Debug("Received parallel mode commit reply of txn: %lu", msg.tid());
    uint64_t tid = msg.tid();
    auto it = pendingParallelCommits.find(tid);
    if (it == pendingParallelCommits.end()) {
        Debug("Received reply when no request was pending");
        return;
    }

    PendingParallelCommitRequest *req =
        dynamic_cast<PendingParallelCommitRequest *>(it->second);

    // delete timer event
    req->timer->Stop();
    // remove from pending list
    pendingParallelCommits.erase(it);

    req->continuation(msg.tid(), msg.state(), msg.timestamp());
    delete req;
}

void 
LRClient::InvokeGetData(
    uint64_t tid,
    const std::vector<std::string> &keys,
    uint64_t replica_id,
    get_continuation_t continuation,
    uint32_t timeout,
    error_continuation_t error_continuation
){
    auto timer = std::unique_ptr<Timeout>(new Timeout(
        transport, timeout,
        [this, tid]() { GetDateTimeoutCallback(tid); }));
    
    PendingGetDataRequest *req = 
    new PendingGetDataRequest(tid, continuation, error_continuation, std::move(timer));

    GetData reqMsg;
    for(auto key : keys){
        reqMsg.add_keys(key);
    }
    reqMsg.set_tid(tid);

    Debug("Send getdata request of txn %lu to replica %lu", tid, replica_id);

    if(transport->SendMessageToReplica(this, replica_id, reqMsg)){
        req->timer->Start();
        pendingReqs[tid] = req;
    } else {
        Warning("Could not send GetData request to replica");
        delete req;
    }
}

void
LRClient::GetDateTimeoutCallback(const uint64_t tid)
{
    auto it = pendingReqs.find(tid);
    if (it == pendingReqs.end()) {
        Debug("Received timeout when no request was pending");
        return;
    }

    PendingGetDataRequest *req = static_cast<PendingGetDataRequest *>(it->second);
    ASSERT(req != NULL);

    Warning("GetData request timed out [%lu]", tid);
    // delete timer event
    req->timer->Stop();
    // remove from pending list
    pendingReqs.erase(it);
    // invoke application callback
    if (req->error_continuation) {
        req->error_continuation(tid, ErrorCode::TIMEOUT);
    }
    delete req;
}

void 
LRClient::HandleGetDataReply(const TransportAddress &remote,
                        const GetDataReply &msg){
    Debug("777777");
    Debug("receive get data reply of txn %lu", msg.tid());
    uint64_t tid = msg.tid();
    auto it = pendingReqs.find(tid);
    if (it == pendingReqs.end()) {
        Debug("Received reply when no request was pending");
        return;
    }
    Debug("8888888888");
    PendingGetDataRequest *req =
        dynamic_cast<PendingGetDataRequest *>(it->second);
    Debug("sbusbusbu");
    // delete timer event
    req->timer->Stop();
    // remove from pending list
    pendingReqs.erase(it);
    Debug("9999999999");
    // invoke application callback
    std::vector<std::pair<string, uint64_t>> readKey;
    std::vector<std::string> values;
    for(auto key : msg.keys()){
        readKey.push_back(make_pair(key.key(), key.timestamp()));
        values.push_back(key.value());
    }
    Debug("1010100101");
    // TODO
    req->continuation(readKey, values, msg.state());
    delete req;
    Debug("20202022020");
}

void
LRClient::InvokeCommit(
    uint64_t tid,
    uint64_t leader,
    LogEntryState state,
    continuation_t continuation,
    error_continuation_t error_continuation
){
    auto timer = std::unique_ptr<Timeout>(new Timeout(
        transport, 1000,
        [this, tid]() { }));
    
    Commit reqMsg;
    reqMsg.set_tid(tid);
    reqMsg.set_state(state);

    if(!(transport->SendMessageToReplica(this, leader, reqMsg))){
        Warning("Could not send Commit request to leader");
    }
}

bool
LRClient::CarouselCheck(PendingPrepare *req){
    if(req->reachFaultTolerance){
        Debug("reach Carousel slow path on shard %d", req->txn.getPrimaryShard());
        return true;
    }

    int superMajority = config.FastQuorumSize();
    
    if(req->replicaReplies.size() >= superMajority){
        int okCount = 0, failCount = 0;
        for(auto it : req->replicaReplies){
            if(it.second == proto::LogEntryState::LOG_STATE_PREPAREOK)
                okCount ++;
            else
                failCount ++;
        }
        Debug("ok %d, fail %d", okCount, failCount);

        auto leaderReply = req->replicaReplies.find(req->leader);

        if(okCount >= superMajority && leaderReply != req->replicaReplies.end()){
            Debug("reach Carousel fast path on shard %d", req->txn.getPrimaryShard());
            req->state = proto::LogEntryState::LOG_STATE_PREPAREOK;
            return true;
        }

        if(failCount >= superMajority && leaderReply != req->replicaReplies.end()){
            req->state = proto::LogEntryState::LOG_STATE_ABORT;
            return true;
        }
    }

    return false;
}

bool
LRClient::RCCheck(PendingPrepare *req){
    int majority = config.QuorumSize();

    if(req->replicaReplies.size() >= majority){
        int okCount = 0, failCount = 0;
        for(auto it : req->replicaReplies){
            if(it.second == proto::LogEntryState::LOG_STATE_PREPAREOK)
                okCount ++;
            else
                failCount ++;
        }
        Debug("ok %d, fail %d", okCount, failCount);

        if(okCount >= majority){
            Debug("reach consensus on final decision of txn %lu", req->tid);
            req->state = proto::LogEntryState::LOG_STATE_PREPAREOK;
            return true;
        }

        if(failCount >= majority){
            req->state = proto::LogEntryState::LOG_STATE_ABORT;
            return true;
        }
    }

    return false;
}

}
}