#include "replication/lr/replica.h"

#include <cstdint>

#include <set>

namespace replication{
namespace lr{
using namespace std;
using namespace proto;

LRReplica::LRReplica(std::vector<transport::Configuration> config, 
                transport::Configuration replica_config,
                transport::Configuration coordinator_config,
                uint64_t replica_id, uint64_t leader, uint64_t shard, int nshards,
                std::vector<Transport *> shardTransport, 
                Transport *transport,
                Transport *coordinator_transport, LRAppReplica *store, 
                Mode mode, TpcMode tpcMode)
    : config(config), coordinator_config(coordinator_config),
    replica_id(replica_id), leader(leader), shard_id(shard), nshards(nshards), shardTransport(shardTransport), 
    coordinator_transport(coordinator_transport), store(store), mode(mode), tpcMode(tpcMode), 
    replica_config(replica_config), transport(transport), log(Log(0)){
    if(replica_id == leader)
        isLeader = true;
    term = 0;
    Debug("shard id: %lu", shard_id);
    transport->Register(this, replica_config, replica_id);
    coordinator_transport->Register(this, coordinator_config, -1);
    for(int i = 0; i < config.size(); i++){
        shardTransport[i]->Register(this, config[i], -1);
        Debug("Register shard %d transport", i);
    }
    // transport = shardTransport[shard_id];
    // replica_config = config[shard_id];
    // log = Log(0);
}

LRReplica::~LRReplica(){
    for(auto kv : pendingTxns){
        delete kv.second;
    }

    for(auto kv : activeTxns){
        delete kv.second;
    }

    for(auto kv : clientList){
        delete kv.second;
    } 

    for(auto kv : prepareClientList){
        delete kv.second;
    }
}

void
LRReplica::ReceiveMessage(const TransportAddress &remote,
                          const string &type, const string &data)
{
    HandleMessage(remote, type, data);
}

void 
LRReplica::HandleMessage(const TransportAddress &remote,
                        const string &type, const string &data){
    PrepareTransaction prepareTxn;
    PrepareTransactionReply prepareTxnReply;
    ReplicateTransaction replicateTxn;
    ReplicateTransactionReply replicateTxnReply;
    Commit commitDecision;
    ReplicateCommit replicateCommit;
    GetData getData;
    ParallelModeCommit parallelModeCommit;

    // message from coordinator replicas
    replication::commit::proto::NotifyDecision notifyDecision;
    replication::commit::proto::NotifyReplicateResult notifyReplicateResult;

    if(type == replicateTxn.GetTypeName()){
        replicateTxn.ParseFromString(data);
        HandleReplicateTransaction(remote, replicateTxn);
    } else if (type == replicateTxnReply.GetTypeName()){
        replicateTxnReply.ParseFromString(data);
        HandleReplicateTransactionReply(remote, replicateTxnReply);
    } else if (type == prepareTxn.GetTypeName()){
        prepareTxn.ParseFromString(data);
        HandlePrepareTxn(remote, prepareTxn);
    } else if (type == prepareTxnReply.GetTypeName()){
        prepareTxnReply.ParseFromString(data);
        HandlePrepareTransactionReply(remote, prepareTxnReply);
    } else if (type == commitDecision.GetTypeName()){
        commitDecision.ParseFromString(data);
        HandleCommit(commitDecision);
    } else if (type == replicateCommit.GetTypeName()){
        replicateCommit.ParseFromString(data);
        HandleReplicateCommit(remote, replicateCommit);
    } else if (type == getData.GetTypeName()){
        getData.ParseFromString(data);
        HandleRead(remote, getData);
    } else if (type == notifyDecision.GetTypeName()){
        notifyDecision.ParseFromString(data);
        HandleDecisionFromCoordinatorReplica(remote, notifyDecision);
    } else if (type == notifyReplicateResult.GetTypeName()){
        notifyReplicateResult.ParseFromString(data);
        HandleReplicateResultFromCoordinatorReplica(remote, notifyReplicateResult);
    } else if (type == parallelModeCommit.GetTypeName()){
        parallelModeCommit.ParseFromString(data);
        HandleParallelModeCommit(remote, parallelModeCommit);
    }
    else {
        Panic("Received unexpected message type: %s", type.c_str());
    }
}


// fast mode
// coordinator -> participant leader
// leader receive prepare req, do concurrency control, then send vote and replicate txn log
void
LRReplica::HandlePrepareTxn(const TransportAddress &remote,
                        const proto::PrepareTransaction &msg){
    Debug("Receive the prepare request of txn: %lu", msg.tid());


    if((msg.mode() == proto::CommitMode::MODE_SLOW || 
        msg.mode() == proto::CommitMode::MODE_RC)&& 
        msg.primaryshard() == shard_id &&
        pendingPrepares.find(msg.tid()) == pendingPrepares.end()){
        HandlePrepare(remote, msg);
        return;
    }

    if(inDependency.find(msg.tid()) != inDependency.end() && inDependency[msg.tid()] == -1) {
        inDependency.erase(msg.tid());
        return;
    }

    auto it = committedTxns.find(msg.tid());
    if(it != committedTxns.end()){
        Debug("Txn %lu already reach final decision %d", msg.tid(), it->second);
        proto::PrepareTransactionReply reqMsg;
        reqMsg.set_tid(msg.tid());
        reqMsg.set_state(it->second.first);
        reqMsg.set_shard(shard_id);
        reqMsg.set_timestamp(it->second.second);
        // reqMsg.set_timestamp(activeTxns[txn_id]->getTimestamp().getTimestamp());
        if(tpcMode == TpcMode::MODE_SLOW){
            int leader = msg.primaryshard() % replica_config.n;
            shardTransport[msg.primaryshard()]->SendMessageToReplica(this, leader, reqMsg);
        } else {
            transport->SendMessage(this, remote, reqMsg);
        }
        return;
    }

    // already reach consensus, but has not received the commit message yet.
    auto it2 = pendingTxns.find(msg.tid());
    if(it2 != pendingTxns.end()){
        PendingTransaction *req =
            dynamic_cast<PendingTransaction *>(it2->second);
        if(req->reach_consensus){
            Debug("Txn %lu already reach consensus, state %d", msg.tid(), req->finalResult);
            proto::PrepareTransactionReply reqMsg;
            reqMsg.set_tid(msg.tid());
            reqMsg.set_state(req->finalResult);
            reqMsg.set_shard(shard_id);
            reqMsg.set_timestamp(activeTxns[msg.tid()]->getTimestamp().getTimestamp());
            // reqMsg.set_timestamp(activeTxns[txn_id]->getTimestamp().getTimestamp());
            if(tpcMode == TpcMode::MODE_SLOW){
                int leader = msg.primaryshard() % replica_config.n;
                shardTransport[msg.primaryshard()]->SendMessageToReplica(this, leader, reqMsg);
            } else {
                if(tpcMode == TpcMode::MODE_CAROUSEL){
                    reqMsg.set_replica(replica_id);
                    reqMsg.set_reachfaulttolerance(1);
                }
                transport->SendMessage(this, remote, reqMsg);
            }
            return;
        }
    }

    Transaction* txn = new Transaction(msg.tid(), increaseTs(), replica_id, msg.primaryshard());

    for(proto::WriteSet wsetEntry : msg.wset()){
        // txn.wset.insert(std::pair<std::string, std::string> (wsetEntry.key(), wsetEntry.value()));
        txn->addWriteSet(wsetEntry.key(), wsetEntry.value());
    }

    for(proto::ReadSet rsetEntry : msg.rset()){
        Timestamp ts = Timestamp(rsetEntry.readts(), rsetEntry.replica());
        txn->addReadSet(rsetEntry.key(), ts);
    }

    for(uint64_t p : msg.participants()){
        txn->addParticipant(p);
    }


    txn->setIn(inDependency[msg.tid()]);
    Debug("Transaction %lu has %lu dependencies", msg.tid(), txn->getIn());
    inDependency.erase(msg.tid());

    
    string request_str;
    strongstore::proto::Request request;
    strongstore::proto::Reply reply;
    string res;
    bool replicate = false;

    request.set_op(strongstore::proto::Operation::PREPARE);
    request.set_txnid(msg.tid());
    txn->serialize(request.mutable_prepare()->mutable_txn());
    request.SerializeToString(&request_str);

    // In Carousel and RC modes, each replica will do the check
    if(isLeader || tpcMode == TpcMode::MODE_CAROUSEL || tpcMode == TpcMode::MODE_RC){
        store->PrepareUpcall(request_str, res, replicate, true);
    } else {
        store->PrepareUpcall(request_str, res, replicate, false);
    }


    if(!replicate){ // fail the concurrency control on the leader, abort
        proto::PrepareTransactionReply reqMsg;
        reqMsg.set_tid(msg.tid());
        reqMsg.set_state(proto::LogEntryState::LOG_STATE_ABORT);
        reqMsg.set_timestamp(0);
        reqMsg.set_shard(shard_id);
        reqMsg.set_replica(replica_id);
        
        if(tpcMode == TpcMode::MODE_CAROUSEL){
            if(isLeader)
                reqMsg.set_reachfaulttolerance(1);
        }

        Debug("Sending prepare reply of txn: %lu", msg.tid());
        if(!(transport->SendMessage(this, remote, reqMsg))){
            Warning("Send abort to coordinator client failed");
        }

        // if(tpcMode == TpcMode::MODE_PARALLEL){
        //     Debug("Send vote of shard %lu to coordinator replicas", shard_id);
        //     replication::commit::proto::Vote vote;
        //     vote.set_tid(msg.tid());
        //     vote.set_vote(replication::commit::proto::CommitResult::ABORT);
        //     vote.set_shard(shard_id);
        //     vote.set_primaryshard(msg.primaryshard());
        //     vote.set_replica(replica_id);
        //     vote.set_timestamp(txn->getTimestamp().getTimestamp());

        //     for(auto p : txn->getParticipants()){
        //         vote.add_participants(p);
        //     }

        //     if(!(coordinator_transport->SendMessageToAll(this, vote))){
        //         Warning("Send abort to coordinator replicas failed");
        //     }
        // }
    } else {
        // In Mode_RC, the prepare req is sent to all shards in the same region.
        txn->getStartTimeOfCriticalPath();
        Debug("Txn %lu enters the critical path", msg.tid());
        if(tpcMode == TpcMode::MODE_RC){
            proto::PrepareTransactionReply reqMsg;
            reqMsg.set_tid(msg.tid());
            reqMsg.set_state(proto::LogEntryState::LOG_STATE_PREPAREOK);
            reqMsg.set_timestamp(txn->getTimestamp().getTimestamp());
            reqMsg.set_shard(shard_id);
            reqMsg.set_replica(replica_id);

            Debug("RC mode, shard %lu send prepare reply to shard %lu", shard_id, msg.primaryshard());

            if(!(shardTransport[msg.primaryshard()]->SendMessageToReplica(this, replica_id, reqMsg))){
                Warning("Send decision to coordinator failed");
                return;
            }
        }

        if(tpcMode == TpcMode::MODE_CAROUSEL){
            proto::PrepareTransactionReply reqMsg;
            reqMsg.set_tid(msg.tid());
            reqMsg.set_state(proto::LogEntryState::LOG_STATE_PREPAREOK);
            reqMsg.set_timestamp(txn->getTimestamp().getTimestamp());
            reqMsg.set_shard(shard_id);
            reqMsg.set_replica(replica_id);

            Debug("Sending prepare reply of txn: %lu", msg.tid());
            if(!(transport->SendMessage(this, remote, reqMsg))){
                Warning("Send decision to coordinator client failed");
            }
        }

        if(isLeader){
            reply.ParseFromString(res);

            clientList[msg.tid()] = remote.clone();
            // clientList[msg.tid()] = make_pair(msg.coordinatorshard(), msg.coordiantorid);

            activeTxns[txn->getID()] = txn;
 
            LogEntry * entry = log.Add(term, txn->getID(), txn->getWriteSet(), 
                                    proto::LogEntryState::LOG_STATE_PREPAREOK, txn->getTimestamp().getTimestamp());

            if(txn->getIn() > 0){
                Debug("Has %lu precommit dependencies.", txn->getIn());
                InvokeReplicateTxn(entry, true);
            }
            else
                InvokeReplicateTxn(entry, false);
        }
    }
}


// Leader adds the txn to be replicated to pendingtxns, sends replicate message to followers
void 
LRReplica::InvokeReplicateTxn(const LogEntry *entry, bool hasPreCommit){
    txn_id tid = entry->txn;
    lsn id = entry->id;

    auto it = pendingTxns.find(tid);
    if(it == pendingTxns.end()){     
        auto timer = std::unique_ptr<Timeout>(new Timeout(
            transport, 1000, [this, tid]() {
                ResendReplicateTxn(tid);
            }
        ));
        
        PendingTransaction *req;
        if(tpcMode == TpcMode::MODE_RC){
            req = new PendingTransaction(
                nullptr, replica_config.QuorumSize(), id, tid,
                activeTxns[tid]->getParticipants(), activeTxns[tid]->getPrimaryShard());
        } else {
            req = new PendingTransaction(
                std::move(timer), replica_config.QuorumSize(), id, tid,
                activeTxns[tid]->getParticipants(), activeTxns[tid]->getPrimaryShard());
        }
        pendingTxns[tid] = req;

        req->has_precommit = hasPreCommit;

        if(tpcMode != TpcMode::MODE_RC){ // in RC mode, it doesn't need to replicate the transaction log
            ReplicateTxn(req);
        }
    } else {
        PendingTransaction *req =
            dynamic_cast<PendingTransaction *>(it->second);

        if(tpcMode != TpcMode::MODE_RC && req->timer == nullptr){
            auto timer = std::unique_ptr<Timeout>(new Timeout(
                transport, 1000, [this, tid]() {
                    ResendReplicateTxn(tid);
                }
            ));
            req->timer = std::move(timer);
            req->lsn = id;
            req->majority = replica_config.QuorumSize();
            req->participants = activeTxns[tid]->getParticipants();
            req->primary_shard = activeTxns[tid]->getPrimaryShard();
            req->has_precommit = hasPreCommit;
            
            ReplicateTxn(req);
        } else {
            ResendReplicateTxn(tid);
        }
    }
}

void 
LRReplica::ResendReplicateTxn(uint64_t tid){
    if(tpcMode == TpcMode::MODE_RC)
        return;
    Warning("Timeout; resending replicate txn request: %lu", tid);
    ReplicateTxn((PendingTransaction *)pendingTxns[tid]);
}   

void
LRReplica::ReplicateTxn(const PendingTransaction *req){
    Debug("Replicate txn %lu to replicas", req->tid);
    proto::ReplicateTransaction replicateTxnMsg;
    proto::LogEntryProto* txnLog = replicateTxnMsg.mutable_txnlog();
    log.EntryToProto(txnLog, req->lsn);

    replicateTxnMsg.set_term(term);
    replicateTxnMsg.set_replicaid(replica_id);
    replicateTxnMsg.set_vote(proto::LogEntryState::LOG_STATE_PREPAREOK);
    replicateTxnMsg.set_hasprecommit(req->has_precommit);
    

    //TODO if the number of replicas of a shard and coordinator is the same as the number data centers
    // i.e., each data center has a replica of each shard and coordinator
    // and the default leader id of each shard = shard_id % replica number
    int primary_coordinator = req->primary_shard % replica_config.n;
    replicateTxnMsg.set_primarycoordinator(primary_coordinator);
    replicateTxnMsg.set_primaryshard(req->primary_shard);
    for(auto p : req->participants){
        replicateTxnMsg.add_participants(p);
    }

    if(transport->SendMessageToAll(this, replicateTxnMsg)){
        req->timer->Reset();
    } else {
        Warning("Could not send replicate request to replicas");
        pendingTxns.erase(req->tid);
        delete req;
    }
}

// follwer receive replicate message from leader
void
LRReplica::HandleReplicateTransaction(const TransportAddress &remote,
                                     const ReplicateTransaction &msg){
    uint64_t lsn = msg.txnlog().lsn();
    uint64_t txn_id = msg.txnlog().txnid();

    Debug("Replica: %lu received replicate message of txn: %lu from replica: %d", replica_id, txn_id, msg.replicaid());

    ASSERT(msg.replicaid() == leader);


    if(msg.txnlog().timestamp() > current_ts)
        current_ts = msg.txnlog().timestamp();

    LogEntry *entry = log.Find(lsn);

    ReplicateTransactionReply reply;

    if(entry != NULL){
        reply.set_lsn(lsn);
        reply.set_term(term);
        reply.set_replicaid(replica_id);
        reply.set_txnid(txn_id);
    } else {

        log.Add(msg.txnlog());

        entry = log.Find(lsn);
        Transaction * txn = new Transaction(txn_id, entry->ts, msg.replicaid());

        for(proto::WriteSet wsetEntry : msg.txnlog().wset()){
            // txn.wset.insert(std::pair<std::string, std::string> (wsetEntry.key(), wsetEntry.value()));
            txn->addWriteSet(wsetEntry.key(), wsetEntry.value());
        }


        for(uint64_t p : msg.participants()){
            txn->addParticipant(p);
        }

        activeTxns[txn->getID()] = txn;

        reply.set_lsn(lsn);
        reply.set_term(term);
        reply.set_replicaid(replica_id);
        reply.set_txnid(txn_id);

        string request_str;
        strongstore::proto::Request request;
        string res;
        bool replicate = false;

        request.set_op(strongstore::proto::Operation::PREPARE);
        request.set_txnid(msg.txnlog().txnid());
        txn->serialize(request.mutable_prepare()->mutable_txn());
        request.SerializeToString(&request_str);

        store->PrepareUpcall(request_str, res, replicate, isLeader);
    }

    if(transport->SendMessage(this, remote, reply)){

    } else {
        Warning("Sending replicate transaction reply failed.");
    }

    if(tpcMode == TpcMode::MODE_PARALLEL){
        replication::commit::proto::ReplicateReplyAndVote reply_vote;
        reply_vote.set_tid(msg.txnlog().txnid());
        reply_vote.set_shard(shard_id);
        reply_vote.set_replica(replica_id);
        reply_vote.set_primarycoordinator(msg.primarycoordinator());
        reply_vote.set_primaryshard(msg.primaryshard());
        reply_vote.set_hasprecommit(msg.hasprecommit());

        if(msg.vote() == proto::LogEntryState::LOG_STATE_PREPAREOK){
            reply_vote.set_vote(replication::commit::proto::CommitResult::PREPARED);
        }

        for(auto p : msg.participants()){
            reply_vote.add_participants(p);
        }


        Debug("Replica %lu sends reply and vote to the co-located coordinator", 
            reply_vote.replica());
        if(!(coordinator_transport->SendMessageToReplica(
            this, replica_id, reply_vote))){
            Warning("Sending reply and vote to co-located coordinator failed");
        }
    }
}


// Leader receive replicate reply and checks if reach consensus on the majority.
void
LRReplica::HandleReplicateTransactionReply(const TransportAddress &remote,
                                     const ReplicateTransactionReply &msg){
    Debug("Replica: %lu received replicate reply of txn: %lu from replica: %d", replica_id, msg.txnid(), msg.replicaid());
    uint64_t txn_id = msg.txnid();

    auto it = pendingTxns.find(txn_id);
    if (it == pendingTxns.end()){
        Debug(
            "Replica was not expecting a ReplicateTxnReply for txn %lu, so ignore it.",
            txn_id
        );
        return;
    }

    PendingTransaction *req =
        dynamic_cast<PendingTransaction *>(it->second);
    ASSERT(req != nullptr);

    if(req->reach_consensus){
        Debug(
            "Txn %lu is already reach a consensus, it is successfully replicated.",
            txn_id
        );
        return;
    }
    
    req->quorum.Add(msg.term(), msg.replicaid(), msg);
    const std::map<int, proto::ReplicateTransactionReply> &msgs =
        req->quorum.GetMessages(msg.term());

    if(msgs.size() >= req->majority){
        req->reach_consensus = true;
        req->finalResult = proto::LogEntryState::LOG_STATE_PREPAREOK;
        HandleConsensus(txn_id, msgs, req);
    }
}



// Leader receive replicate replies from the majority.
void 
LRReplica::HandleConsensus(const uint64_t txn_id,
                        const std::map<int, proto::ReplicateTransactionReply> &msgs,
                        PendingTransaction *req){
    proto::PrepareTransactionReply reqMsg;
    reqMsg.set_tid(txn_id);
    reqMsg.set_state(proto::LogEntryState::LOG_STATE_PREPAREOK);
    reqMsg.set_timestamp(activeTxns[txn_id]->getTimestamp().getTimestamp());
    reqMsg.set_shard(shard_id);
    reqMsg.set_replica(replica_id);

    Debug("Txn %lu reach consensus on shard %lu, reply the the client by replica %lu", txn_id, reqMsg.shard(), reqMsg.replica());

    if(tpcMode == TpcMode::MODE_SLOW){
        int primaryShardLeader = req->primary_shard % replica_config.n;
        Debug("Primary shard %lu, leader %d", req->primary_shard, primaryShardLeader);

        if(shardTransport[req->primary_shard]->SendMessageToReplica(this, primaryShardLeader, reqMsg)){
            req->timer = std::unique_ptr<Timeout>(new Timeout(
                transport, 1000, [this, txn_id]() {
                    ResendPrepareOK(txn_id);
                }
            ));
        } else {
            Warning("Send prepare ok to coordinator failed");
            pendingTxns.erase(txn_id);
            delete req;

            Transaction *txn = activeTxns[txn_id];
            activeTxns.erase(txn_id);
            if(txn != NULL){
                delete txn;
            }
        }
        return;
    }

    if(tpcMode == TpcMode::MODE_PARALLEL){
        auto it = pendingTxns.find(txn_id);
        if (it == pendingTxns.end()){
            return;
        }

        PendingTransaction *req =
            dynamic_cast<PendingTransaction *>(it->second);
        ASSERT(req != nullptr);

        if(!req->has_precommit)
            return;
    }

    if(tpcMode == TpcMode::MODE_CAROUSEL){
        reqMsg.set_replica(replica_id);
        reqMsg.set_reachfaulttolerance(1);
    }

    if(transport->SendMessage(this, *clientList[txn_id], reqMsg)){
        Debug("Send prepare ok of %lu success", txn_id);
        auto timer = std::unique_ptr<Timeout>(new Timeout(
            transport, 1000, [this, txn_id]() {
                ResendPrepareOK(txn_id);
            }
        ));
        req->timer = std::move(timer);
        req->timer->Reset();
    } else {
        Warning("Send prepare ok to coordinator failed");
        pendingTxns.erase(txn_id);
        delete req;


        Transaction *txn = activeTxns[txn_id];
        activeTxns.erase(txn_id);
        if(txn != NULL){
            delete txn;
        }

        TransportAddress * addr = clientList[txn_id];
        clientList.erase(txn_id);
        if(addr != NULL){
            delete addr;
        }
    }
}

void
LRReplica::ResendPrepareOK(uint64_t tid){
    Debug("Timeout, resend prepareok of txn %lu", tid);
    auto it = pendingTxns.find(tid);
    if (it == pendingTxns.end()){
        return;
    }

    PendingTransaction *req =
        dynamic_cast<PendingTransaction *>(it->second);
    ASSERT(req != nullptr);

    proto::PrepareTransactionReply reqMsg;
    reqMsg.set_tid(tid);
    reqMsg.set_state(proto::LogEntryState::LOG_STATE_PREPAREOK);
    reqMsg.set_timestamp(activeTxns[tid]->getTimestamp().getTimestamp());
    reqMsg.set_shard(shard_id);

    if(tpcMode == TpcMode::MODE_SLOW){
        int primaryShardLeader = req->primary_shard % replica_config.n;
        if(shardTransport[req->primary_shard]->SendMessageToReplica(this, primaryShardLeader, reqMsg)){
            req->timer->Reset();
        } else {
            Warning("Send prepare ok to primary shard %lu failed", req->primary_shard);
        }
        return;
    }

    if(transport->SendMessage(this, *clientList[tid], reqMsg)){
        req->timer->Reset();
    } else {
        Warning("Send prepare ok to coordinator failed");
    }
}

// leader receive commit from coordinator
void 
LRReplica::HandleCommit(const proto::Commit &msg){
    Debug("Receive the commit decision of txn: %lu, state: %d", msg.tid(), msg.state());

    auto it = pendingTxns.find(msg.tid());
    if (it == pendingTxns.end()){
        Debug(
            "Replica was not expecting a Commit for txn %lu, so ignore it.",
            msg.tid()
        );
        return;
    }

    if(msg.timestamp() > current_ts){
        Debug("Find a larger ts, updata my ts to %lu", msg.timestamp());
        current_ts = msg.timestamp();
    }

    PendingTransaction *req =
        dynamic_cast<PendingTransaction *>(it->second);
    ASSERT(req != nullptr);


    if(!req->is_participant){
        Debug("The primary shard is not in participants"); 
        pendingTxns.erase(msg.tid());
        delete req;  
    } else {
        LogEntry *entry = log.Find(req->lsn);
        entry->ts = msg.timestamp();

        if(!req->send_confirms){
            notifyCommitToReplica(req, msg);
        }

        if(!req->reach_commit_decision){
            applyToStore(req, msg);
        }

        Transaction *txn = activeTxns[msg.tid()];

        if(msg.state() == replication::commit::proto::CommitResult::COMMIT) {
            notifyForFollowingDependenies(msg.tid());
        } else {
            CascadeAbort(msg.tid());
        }

        pendingTxns.erase(msg.tid());
        delete req;

        if(tpcMode != TpcMode::MODE_PARALLEL){
            txn->getEndTimeOfCriticalPath();
            Debug("Transaction critical path length: %lu", txn->calculateCriticalPathLen());
        }

        activeTxns.erase(msg.tid());
        if(txn != NULL){
            delete txn;
        }

        committedTxns[msg.tid()] = std::make_pair(msg.state(), msg.timestamp());
    }

    TransportAddress * addr = clientList[msg.tid()];
    clientList.erase(msg.tid());
    if(addr != NULL){
        delete addr;
    }
}

void
LRReplica::CascadeAbort(uint64_t tid){
    string request_str;
    string res;
    strongstore::proto::Request request;
    request.set_txnid(tid);
    request.set_op(strongstore::proto::Operation::ABORT);
    request.SerializeToString(&request_str);
    store->AbortUpcall(request_str, res);

    auto it = pendingTxns.find(tid);
    if (it == pendingTxns.end()){
        return;
    }

    PendingTransaction *req =
        dynamic_cast<PendingTransaction *>(it->second);
    ASSERT(req != nullptr);

    Transaction *txn = activeTxns[tid];

    Debug("Txn %lu is aborting, out dependency size %d", tid, txn->getOut().size());
    for(uint64_t out : txn->getOut()){
        if(activeTxns.find(out)!=activeTxns.end()){
            CascadeAbort(out);

            replication::commit::proto::Vote vote;
            vote.set_tid(tid);
            vote.set_vote(replication::commit::proto::CommitResult::PREPARED);
            vote.set_shard(shard_id);

            if(!(coordinator_transport->SendMessageToAll(this, vote))){
                Warning("Send abort to coordinator replicas failed");
            }
        } else {
            inDependency[out] = -1;
        }
    }
}

// when committing a transaction, notify all out dependencies to decrease their in counter
void
LRReplica::notifyForFollowingDependenies(uint64_t tid){
    auto it = pendingTxns.find(tid);
    if (it == pendingTxns.end()){
        return;
    }

    PendingTransaction *req =
        dynamic_cast<PendingTransaction *>(it->second);
    ASSERT(req != nullptr);


    Transaction *txn = activeTxns[tid];

    Debug("Txn %lu notify following transactions to commit, out dependency size %d.", tid, txn->getOut().size());
    for(uint64_t out : txn->getOut()){
        if(activeTxns.find(out)!=activeTxns.end()){
            Transaction *out_txn = activeTxns[out];
            out_txn->decreaseIn();
            Debug("Decrease the in dependency counter of txn %lu is %lu", out, out_txn->getIn());
            if(out_txn->getIn()==0){
                replication::commit::proto::NotifyDependenciesAreCleared notification;
                notification.set_tid(out);
                notification.set_shard(shard_id);

                Debug("Txn %lu is committing, notifies txn %lu to commit", tid, out);

                int coorspondent_coordinator = req->primary_shard % replica_config.n;
                if(!(coordinator_transport->SendMessageToReplica(
                    this, coorspondent_coordinator, notification))){
                    Warning("Notify for dependencies are cleared for txn %lu failed", tid);
                }
            }
        } else {
            inDependency[out]--;
            Debug("The in dependency counter of txn %lu is %lu", out, inDependency[out]);
        }
    }
}

void
LRReplica::notifyCommitToReplica(PendingTransaction *req, const proto::Commit &msg){
    log.SetStatus(req->tid, msg.state());
    // notify replicas to commit the log of txn and apply its writes to the store
    proto::ReplicateCommit reqMsg;
    reqMsg.set_lsn(req->lsn);
    reqMsg.set_state(msg.state());
    reqMsg.set_tid(msg.tid());
    reqMsg.set_timestamp(msg.timestamp());

    // In RC mode, the commit message is sent to participant replicas in the same region
    if(tpcMode == TpcMode::MODE_RC){
        for(auto p : req->participants){
            if(!(shardTransport[p]->SendMessageToReplica(this, replica_id, reqMsg))){
                Warning("Send commit decision to participants failed");
            }
        }
    } else { // In fast, slow, parallel and carousel modes, the commit message is replicated to all replicas
        if(transport->SendMessageToAll(this, reqMsg)){
            req->send_confirms = true;
        } else {
            Warning("Send commit decision to replicas failed");
        }
    }
}


void
LRReplica::applyToStore(PendingTransaction *req, const proto::Commit &msg){
    req->reach_commit_decision = true;
    string request_str;
    strongstore::proto::Request request;
    strongstore::proto::Reply reply;
    string res;

    request.set_txnid(msg.tid());
    Transaction* txn = activeTxns[msg.tid()];

    switch (msg.state())
    {
    case proto::LogEntryState::LOG_STATE_COMMIT:
        // request.mutable_commit()->set_timestamp(txn->getTimestamp().getTimestamp());
        if(msg.timestamp() == 0){
            Warning("Commit Timestamp is unset");
        }
        request.mutable_commit()->set_timestamp(msg.timestamp());
        request.set_op(strongstore::proto::Operation::COMMIT);
        request.SerializeToString(&request_str);

        store->CommitUpcall(request_str, res);
        break;
    case proto::LogEntryState::LOG_STATE_ABORT:
        request.set_op(strongstore::proto::Operation::ABORT);
        txn->serialize(request.mutable_abort()->mutable_txn());
        request.SerializeToString(&request_str);

        store->AbortUpcall(request_str, res);
        break;
    default:
        break;
    }

    reply.ParseFromString(res);
}

// In parallel mode, if a txn is committed and there exists a transaction which reads its updates and is blocked to vote, it will send vote for this transaction.
void
LRReplica::VoteForNext(std::vector<uint64_t> nexts){
    for(uint64_t tid : nexts){
        auto it = pendingTxns.find(tid);
        if (it == pendingTxns.end()){
            return;
        }

        PendingTransaction *req =
            dynamic_cast<PendingTransaction *>(it->second);
        ASSERT(req != nullptr);

        Debug("Send vote of txn %lu on shard %lu to coordinator replicas", tid, shard_id);
        replication::commit::proto::Vote vote;
        vote.set_tid(tid);
        vote.set_vote(replication::commit::proto::CommitResult::PREPARED);
        vote.set_shard(shard_id);
        vote.set_primaryshard(req->primary_shard);
        vote.set_replica(replica_id);

        for(auto p : req->participants){
            vote.add_participants(p);
        }

        if(!(coordinator_transport->SendMessageToAll(this, vote))){
            Warning("Send abort to coordinator replicas failed");
        }

        if(req->reach_consensus){
            proto::PrepareTransactionReply reqMsg;
            reqMsg.set_tid(tid);
            reqMsg.set_state(proto::LogEntryState::LOG_STATE_PREPAREOK);
            reqMsg.set_timestamp(activeTxns[tid]->getTimestamp().getTimestamp());
            reqMsg.set_shard(shard_id);
            reqMsg.set_replica(replica_id);

            Debug("Txn %lu reach consensus on shard %lu, reply the the client by replica %lu", tid, shard_id, replica_id);

            if(clientList.find(tid) == clientList.end()){
                Debug("Wrong");
            } else {

                if(transport->SendMessage(this, *clientList[tid], reqMsg)){
                    Debug("Send prepare ok of %lu success", tid);
                    auto timer = std::unique_ptr<Timeout>(new Timeout(
                        transport, 1000, [this, tid]() {
                            ResendPrepareOK(tid);
                        }
                    ));
                    req->timer = std::move(timer);
                    req->timer->Reset();
                } else {
                    Warning("Send prepare ok to coordinator failed");
                    pendingTxns.erase(tid);
                    delete req;


                    Transaction *txn = activeTxns[tid];
                    activeTxns.erase(tid);
                    if(txn != NULL){
                        delete txn;
                    }

                    TransportAddress * addr = clientList[tid];
                    clientList.erase(tid);
                    if(addr != NULL){
                        delete addr;
                    }
                }
            }
        }
    }
}

// follower receives commit from leader
void
LRReplica::HandleReplicateCommit(const TransportAddress &remote,
                    const proto::ReplicateCommit &msg){
    Debug("Receive commit decision from leader of txn %lu", msg.tid());
    if(isLeader && tpcMode != TpcMode::MODE_RC)
        return;

    if(tpcMode == TpcMode::MODE_RC){ // in RC mode, there has no log entries.
        uint64_t lsn = msg.lsn();
        log.SetStatus(lsn, msg.state());
    }

    // proto::ReplicateCommitReply reqMsg;
    // reqMsg.set_tid(msg.tid());
    // reqMsg.set_state(proto::ReplyState::REPLY_OK);

    // transport->SendMessage(this, remote, reqMsg);

    string request_str;
    strongstore::proto::Request request;
    string res;

    request.set_txnid(msg.tid());
    Transaction* txn = activeTxns[msg.tid()];
    if(txn == NULL){
        Debug("Has not received replicate request of txn: %lu", msg.tid());
        return;
    }

    switch (msg.state())
    {
    case proto::LogEntryState::LOG_STATE_COMMIT:
        // request.mutable_commit()->set_timestamp(txn->getTimestamp().getTimestamp());
        request.mutable_commit()->set_timestamp(msg.timestamp());
        request.set_op(strongstore::proto::Operation::COMMIT);
        request.SerializeToString(&request_str);

        store->CommitUpcall(request_str, res);
        break;
    case proto::LogEntryState::LOG_STATE_ABORT:
        request.set_op(strongstore::proto::Operation::ABORT);
        txn->serialize(request.mutable_abort()->mutable_txn());
        request.SerializeToString(&request_str);

        store->AbortUpcall(request_str, res);
        break;
    default:
        break;
    }

    activeTxns.erase(msg.tid());
    delete txn;
}

void 
LRReplica::HandleRead(const TransportAddress &remote,
                        const proto::GetData &msg){
    Debug("Receive read request of txn: %lu", msg.tid());
    proto::GetDataReply reply;
    reply.set_tid(msg.tid());
    reply.set_replicaid(replica_id);
    bool success = true;

    for(auto key : msg.keys()){
        string request_str;
        string res;
        strongstore::proto::Request request;
        request.set_op(strongstore::proto::Operation::GET);
        request.set_txnid(msg.tid());
        request.mutable_get()->set_key(key);
        request.SerializeToString(&request_str);

        store->GetUpcall(request_str, res);

        strongstore::proto::Reply rep;
        rep.ParseFromString(res);

        if(rep.status() == 0){
            proto::ReadKeyVersion* key_version = reply.add_keys();
            key_version->set_key(key);
            
            key_version->set_timestamp(rep.timestamp());

            if(tpcMode == TpcMode::MODE_PARALLEL){
                if(rep.dependency() > 0){
                    Debug("Read precommit txn %lu", rep.dependency());
                    Transaction *dependent_txn = activeTxns[rep.dependency()];
                    if(dependent_txn != NULL){
                        unordered_map<string, string> wset = dependent_txn->getWriteSet();
                        if(wset.find(key) != wset.end()){
                            string val = wset[key];
                            key_version->set_value(val);
                        }
                        dependent_txn->addOut(msg.tid());
                        Debug("Txn %lu add out dependency %lu", dependent_txn->getID(), msg.tid());
                        inDependency[msg.tid()] ++;
                    }
                } else {
                    string val = rep.value();
                    key_version->set_value(val);
                }
            } else {
                key_version->set_value(rep.value());
            }
        } else {
            success = false;
            break;
        }
    }

    if(success){
        reply.set_state(proto::ReplyState::REP_OK);
    } else {
        reply.set_state(proto::ReplyState::REP_FAIL);
    }
    
    transport->SendMessage(this, remote, reply);
    Debug("[Shard %lu] send get data reply of txn %lu back", shard_id, msg.tid());
}

// Receive decision from the co-coordinator, end concurrency control early
void 
LRReplica::HandleDecisionFromCoordinatorReplica(const TransportAddress &remote,
                                    const replication::commit::proto::NotifyDecision &msg){
    Debug("Receive the commit decision of txn %lu from coordinator replica.", msg.tid());
    ASSERT(tpcMode == TpcMode::MODE_PARALLEL);

    if(replica_id != leader)
        Warning("Error occurs, the commit decision is sent to a normal replica");
    
    auto it = pendingTxns.find(msg.tid());
    if (it == pendingTxns.end()){
        Debug(
            "Replica was not expecting a Commit for txn %lu, so ignore it.",
            msg.tid()
        );
        return;
    }

    PendingTransaction *req =
        dynamic_cast<PendingTransaction *>(it->second);
    ASSERT(req != nullptr);

    // leader can end the transaction early
    // i.e., release lock or the the occ validation early 
    if(msg.state() == replication::commit::proto::CommitResult::PREPARED) {
        PreCommit(req->tid);
    } else {
        CascadeAbort(msg.tid());

        pendingTxns.erase(msg.tid());
        delete req;
    }

    Transaction* txn = NULL;
    if(activeTxns.find(msg.tid()) != activeTxns.end()){
        txn = activeTxns[msg.tid()];
        txn->getEndTimeOfCriticalPath();
        Debug("Txn %lu critical path length: %lu", msg.tid(), txn->calculateCriticalPathLen());
    }

    committedTxns[msg.tid()] = std::make_pair(proto::LogEntryState::LOG_STATE_COMMIT, msg.timestamp());
}


void
LRReplica::PreCommit(const uint64_t tid){
    Debug("PreCommit txn %lu", tid);

    string request_str;
    string res;
    strongstore::proto::Request request;
    request.set_op(strongstore::proto::Operation::PRECOMMIT);
    request.set_txnid(tid);
    request.SerializeToString(&request_str);

    store->PreCommitUpcall(request_str, res);
}

// if correspondent coordinator notify to replicate result, the final decision is determined, 
// the leader can execute the commit phase
void 
LRReplica::HandleReplicateResultFromCoordinatorReplica(const TransportAddress &remote,
                                                    const replication::commit::proto::NotifyReplicateResult &msg){
    Debug("Receive the replicate result of txn %lu from correspondent coordinator.", msg.tid());
    ASSERT(tpcMode == TpcMode::MODE_PARALLEL);

    if(replica_id != leader)
        Warning("Error occurs, the replicate result is sent to a normal replica");

    proto::Commit reqMsg;
    reqMsg.set_tid(msg.tid());
    reqMsg.set_state(proto::LogEntryState::LOG_STATE_COMMIT);
    reqMsg.set_timestamp(msg.timestamp());

    HandleParallelCommitResult(msg.tid(), msg.timestamp(), proto::LogEntryState::LOG_STATE_COMMIT);
    HandleCommit(reqMsg);
}


// If a transaction's primary shard is not in its participants, the primary shard will receive a parallelmodecommit request.
void
LRReplica::HandleParallelModeCommit(const TransportAddress &remote,
                                    const proto::ParallelModeCommit &msg){
    Debug("Receive parallel mode commit request of txn %lu", msg.tid());
    ASSERT(tpcMode == TpcMode::MODE_PARALLEL);
    auto it = pendingTxns.find(msg.tid());
    if(it == pendingTxns.end()){
        PendingTransaction *req = new PendingTransaction(true, replica_config.QuorumSize());
        req->tid = msg.tid();
        req->lsn = -1;
        req->is_participant = false;
        pendingTxns[msg.tid()] = req; 

        Debug("Add txn %lu to pendingTxns list");

        clientList[msg.tid()] = remote.clone();

        replication::commit::proto::RequestCommitResult reqMsg;
        reqMsg.set_tid(msg.tid());
        for(auto p : msg.participants()){
            reqMsg.add_participants(p);
        }

        if(!(coordinator_transport->SendMessageToReplica(this, replica_id, reqMsg))){
            Warning("Send RequestCommitResult to the correspondent coordinator failed.");
        }
    } else {
        PendingTransaction *req =
            dynamic_cast<PendingTransaction *>(it->second);
        req->parallel_mode = true;
    }
}

// received commit from co-coordinator
// notify the parallel commit result to the client
void
LRReplica::HandleParallelCommitResult(uint64_t tid, uint64_t timestamp, proto::LogEntryState res){
    Debug("Sending parallel mode commit result of txn %lu to client", tid);
    proto::ParallelModeCommitReply reply;
    reply.set_tid(tid);
    reply.set_state(res);
    reply.set_timestamp(timestamp);

    auto it = pendingTxns.find(tid);
    
    if (it == pendingTxns.end()){
        Debug(
            "Replica was not expecting a ParallelCommitResult for txn %lu, so ignore it.",
            tid
        );
        return;
    }
    transport->SendMessage(this, *clientList[tid], reply);
    PendingTransaction *req =
            dynamic_cast<PendingTransaction *>(it->second);

    TransportAddress * addr = clientList[tid];
    if(addr != NULL){
        Debug("earse addr of tid: %lu", tid);
        clientList.erase(tid);
        delete addr;
    }
}

/******************** SLOW MODE ********************/


// slow mode and RC mode
// slow mode: client -> primary shard leader
// RC mode: client -> all replicas of primary shard
void
LRReplica::HandlePrepare(const TransportAddress &remote,
                        const proto::PrepareTransaction &msg)
{
    uint64_t tid = msg.tid();
    Debug("Shard %lu receives the prepare request of txn %lu", shard_id, tid);
    Debug("Primary shard %lu", msg.primaryshard());

    prepareClientList[msg.tid()] = remote.clone();

    auto it = pendingPrepares.find(msg.tid());
    if(it == pendingPrepares.end()){
        Transaction txn = Transaction(tid);

        for(proto::WriteSet k : msg.wset()){
            txn.addWriteSet(k.key(), k.value());
        }
        for(proto::ReadSet k : msg.rset()){
            txn.addReadSet(k.key(), Timestamp(k.readts(), k.replica()));
        }
        for(auto p : msg.participants()){
            txn.addParticipant(p);
        }

  
        auto timer = std::unique_ptr<Timeout>(new Timeout(
                    transport, 1000, [this, tid]() {
                        ResendPrepare(tid);
                    }
                ));
        
        PendingPrepare *req = new PendingPrepare(std::move(timer), 
            txn.getParticipants(), tid, txn);

        pendingPrepares[tid] = req;

        SendPrepare(req);
    } else {
        PendingPrepare *req =
            dynamic_cast<PendingPrepare *>(it->second);
        SendPrepare(req);
    }
}

void 
LRReplica::ResendPrepare(uint64_t tid){
    Warning("Timeout; resending prepare request of txn: %lu", tid);
    SendPrepare((PendingPrepare *)pendingPrepares[tid]);
}

// Devides the write and read set of the whole transaction into pieces, each pieces contain the 
// write and read set of a participant.
// In slow mode, the req will send to each participant leader
// In RC mode, the req will send to the participant replica in the same region
void
LRReplica::SendPrepare(const PendingPrepare *req){
    Debug("Send prepare req of txn %lu to participant leader", req->tid);
    std::unordered_map<uint64_t, proto::PrepareTransaction> reqMsgs;
    for(auto p : req->participants){
        proto::PrepareTransaction reqMsg;
        reqMsg.set_tid(req->tid);
        reqMsg.set_primaryshard(shard_id);

        reqMsgs[p] = reqMsg;
    }

    for(auto k : req->txn.getWriteSet()){
        int shard = key_to_shard(k.first);

        proto::WriteSet *wsetEntry = reqMsgs[shard].add_wset();

        wsetEntry->set_key(k.first);
        wsetEntry->set_key(k.second);
    }

    for(auto k : req->txn.getReadSet()){
        int shard = key_to_shard(k.first);
        proto::ReadSet *rsetEntry = reqMsgs[shard].add_rset();

        rsetEntry->set_key(k.first);
        rsetEntry->set_replica(k.second.getID());
        rsetEntry->set_readts(k.second.getTimestamp());
    }

    for(auto p : reqMsgs){
        int sendto = -1;
        if(tpcMode == TpcMode::MODE_SLOW){
            sendto = p.first % replica_config.n;
        } else if (tpcMode == TpcMode::MODE_RC){
            sendto = replica_id;
        }
        Debug("Send to %d", sendto);

        if(shardTransport[p.first]->SendMessageToReplica(this, sendto, p.second)){
            req->timer->Reset();
        } else {
            Warning("Could not send Prepare request to shard %lu leader", p.first);
            pendingPrepares.erase(req->tid);
            delete req;
        }
    }
}

void
LRReplica::HandlePrepareTransactionReply(const TransportAddress &remote,
                                        const proto::PrepareTransactionReply &msg)
{
    Debug("Receive prepare reply of txn: %lu on shard: %lu from replica %lu", msg.tid(), msg.shard(), msg.replica());
    Debug("Shard id %lu, %lu", msg.shard(), shard_id);
    Debug("Replica id %lu, %lu", msg.replica(), replica_id);

    uint64_t tid = msg.tid();
    auto it = pendingPrepares.find(tid);
    if(it == pendingPrepares.end()){
        Debug("Received the prepare reply of a unknown transaction");
        return;
    }

    PendingPrepare *req =
            dynamic_cast<PendingPrepare *>(it->second);

    // for RC mode, to check if the decisions made by different regions can reach consensus.
    // if more than majority regions make the same decision, the final decision is determined.
    // then can notify other replicas to enter the commit phase
    if(msg.shard() == shard_id && msg.replica() != replica_id){
        req->region_replicies[msg.replica()] = msg.state();
        if(msg.timestamp() > req->timestamp)
            req->timestamp = msg.timestamp();
        
        if(ReachConsensusOnFinalDecision(req)){
            req->timer->Stop();
            proto::Commit commitReqMsg;
            commitReqMsg.set_tid(req->tid);
            commitReqMsg.set_state(req->finalDecision);
            commitReqMsg.set_timestamp(req->timestamp);

            HandleCommit(commitReqMsg);

            pendingPrepares.erase(req->tid);
            delete req;

            TransportAddress * addr = prepareClientList[req->tid];
            if(addr != NULL){
                prepareClientList.erase(req->tid);
                delete addr;
            }
        }
        return;
    }

    if(msg.state() == proto::LogEntryState::LOG_STATE_PREPAREOK){
        if(req->participant_replies.find(msg.shard()) != req->participant_replies.end())
            return;
        req->participant_replies[msg.shard()] = msg.state();
        if(msg.timestamp() > req->timestamp)
            req->timestamp = msg.timestamp();
        
        if(ReachFinalDecision(req)){
            req->timer->Stop();
            req->finalDecision = proto::LogEntryState::LOG_STATE_PREPAREOK;
            ReplyFinalDecision(req);
        }
    } else {
        req->finalDecision = proto::LogEntryState::LOG_STATE_ABORT;
        ReplyFinalDecision(req);
    }
}

void
LRReplica::ReplyFinalDecision(PendingPrepare *req)
{
    Debug("Transaction %lu has reach final decision, reply to the client.", req->tid);
    proto::PrepareTransactionReply reqMsg;
    reqMsg.set_tid(req->tid);
    reqMsg.set_state(req->finalDecision);
    reqMsg.set_timestamp(req->timestamp);
    reqMsg.set_shard(shard_id);
    reqMsg.set_replica(replica_id);

    if(!(transport->SendMessage(this, *prepareClientList[req->tid], reqMsg))){
        Warning("Notify the client the final decision failed.");
    }

    // besides reply to the client, in RC mode, the primary shard replica also replicates
    // the decision to all replicas of the primary shard.
    if(tpcMode == TpcMode::MODE_RC){
        req->region_replicies[replica_id] = req->finalDecision;
        if(transport->SendMessageToAll(this, reqMsg)){
            req->timer->Stop();
        } else {
            Warning("Notify the decision to primary shard replicas failed.");
        }
    }

    if(tpcMode == TpcMode::MODE_SLOW){

        proto::Commit commitReqMsg;
        commitReqMsg.set_tid(req->tid);
        commitReqMsg.set_state(req->finalDecision);
        commitReqMsg.set_timestamp(req->timestamp);

        HandleCommit(commitReqMsg);

        pendingPrepares.erase(req->tid);
        delete req;

        TransportAddress * addr = prepareClientList[req->tid];
        if(addr != NULL){
            prepareClientList.erase(req->tid);
            delete addr;
        }
    }
}

// check if receive vote from all participants, and make the decision
bool
LRReplica::ReachFinalDecision(PendingPrepare *req)
{
    if(req->participant_replies.size() < req->participants.size())
        return false;
    
    for(uint64_t p : req->participants){
        if(req->participant_replies[p] != proto::LogEntryState::LOG_STATE_PREPAREOK){
            return false;
        }
    }

    return true;
}

bool
LRReplica::ReachConsensusOnFinalDecision(PendingPrepare *req){
    int majority = replica_config.QuorumSize();

    if(req->region_replicies.size() >= majority){
        int okCount = 0, failCount = 0;
        for(auto it : req->region_replicies){
            if(it.second == proto::LogEntryState::LOG_STATE_PREPAREOK)
                okCount ++;
            else
                failCount ++;
        }

        if(okCount >= majority){
            Debug("reach consensus on final decision of txn %lu", req->tid);
            req->finalDecision = proto::LogEntryState::LOG_STATE_PREPAREOK;
            return true;
        }

        if(failCount >= majority){
            req->finalDecision = proto::LogEntryState::LOG_STATE_ABORT;
            return true;
        }
    }

    return false;
}

// void
// LRReplica::HandleReadOnly(const TransportAddress &remote,
//                         const proto::ReadOnly &msg)
// {
//     Debug("Receive read only txn %lu", msg.tid());

//     for(auto pendindTxn : pendingTxns){
//         if(CheckROIntersectWithPendingRW(msg, pendingTxn->lsn)){

//         }
//     }
// }

// void
// LRReplica::CheckROIntersectWithPendingRW(const proto::ReadOnly &msg, uint64_t lsn){
//     LogEntry *entry = log.Find(lsn);
//     for(auto k : msg.keys()){
//         if(entry->wset.find(k) != entry->wset.end())
//             return true;
//     }
//     return false;
// }

}
}