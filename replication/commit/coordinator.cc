#include "replication/commit/coordinator.h"

namespace replication{
namespace commit{

CoordinatorReplica::CoordinatorReplica(std::vector<transport::Configuration> config, 
    transport::Configuration coordinator_config,
    uint64_t coordinator_id, std::vector<Transport *> shardTransport, Transport *transport) :
    config(config), coordinator_config(coordinator_config), transport(transport), 
    shardTransport(shardTransport), coordinator_id(coordinator_id)
{
    nreplicas = coordinator_config.n;
    quorum_size = nreplicas / 2 + 1;
    Debug("coordinator id: %lu", coordinator_id);
    transport->Register(this, coordinator_config, coordinator_id);
    Debug("shards num: %lu", config.size());
    for(int i = 0; i < config.size(); i++){
        shardTransport[i]->Register(this, config[i], -1);
        Debug("Register shard %d transport", i);
    }
}

CoordinatorReplica::~CoordinatorReplica(){
    for(auto kv : pendingCommits){
        delete kv.second;
    }

    for(auto kv : commitResults){
        delete kv.second;
    }

    // for(auto kv : primaryShardLeaderList){
    //     delete kv.second;
    // }

    // for(auto list : closestLeadersList){
    //     for(auto v : list.second){
    //         delete v;
    //     }
    // }
}

void
CoordinatorReplica::ReceiveMessage(const TransportAddress &remote,
                          const string &type, const string &data)
{
    HandleMessage(remote, type, data);
}

void 
CoordinatorReplica::HandleMessage(const TransportAddress &remote,
                        const string &type, const string &data){
    proto::Vote vote;
    proto::ReplicateResult replicateResult;
    proto::RequestCommitResult requestCommitResult;
    proto::NotifyDecisionToPrimary notDecToPrimary;

    proto::ReplicateReplyAndVote replyAndVote;
    proto::BypassLeaderReply bypassLeaderReply;
    proto::ReplicateDecision replicateDecision;
    proto::ReplyDecision replyDecision;
    proto::BypassLeaderReplyUnion bypassLeaderReplyUnion;

    if(type == replyAndVote.GetTypeName()){
        replyAndVote.ParseFromString(data);
        HandleReplicateReplyAndVote(remote, replyAndVote);
    } else if (type == bypassLeaderReply.GetTypeName()){
        bypassLeaderReply.ParseFromString(data);
        HandleBypassLeaderReply(remote, bypassLeaderReply);
    } else if (type == replicateDecision.GetTypeName()){
        replicateDecision.ParseFromString(data);
        HandleReplicateDecision(remote, replicateDecision);
    } else if (type == replyDecision.GetTypeName()){
        replyDecision.ParseFromString(data);
        HandleReplyDecision(remote, replyDecision);
    } else if (type == bypassLeaderReplyUnion.GetTypeName()){
        bypassLeaderReplyUnion.ParseFromString(data);
        HandleBypassLeaderReplyUnion(remote, bypassLeaderReplyUnion);
    } else if(type == vote.GetTypeName()){
        vote.ParseFromString(data);
        HandleVote(remote, vote);
    } else if (type == requestCommitResult.GetTypeName()){
        requestCommitResult.ParseFromString(data);
        HandleRequestCommitResult(remote, requestCommitResult);
    } else if (type == notDecToPrimary.GetTypeName()){
        notDecToPrimary.ParseFromString(data);
        HandleNotifyDecision(remote, notDecToPrimary);
    } else if (type == replicateResult.GetTypeName()){
        replicateResult.ParseFromString(data);
        Debug("Receive replicate result from shard %lu replica %lu", replicateResult.shard(),
        replicateResult.replica());
    }
    else {
        Panic("Received unexpected message type: %s", type.c_str());
    }
}



// collect votes and check if reach commit, and transfer the replicate reply to correspondent coordinator
void 
CoordinatorReplica::HandleReplicateReplyAndVote(const TransportAddress &remote,
                                const proto::ReplicateReplyAndVote &msg)
{
    Debug("Received the replicate reply and vote of txn %lu from shard %lu replica %lu, vote: %d", msg.tid(), msg.shard(), msg.replica(), msg.vote());
    
    // transaction has already been made the decision.
    if(commitResults.find(msg.tid()) != commitResults.end()){
        Debug("Transaction %lu has already been made the decision, ignore the request.", msg.tid());
        return;
    }

    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        // auto timer = std::unique_ptr<Timeout>(new Timeout(
        //     transport, 2000, [this]() {  }));

        req = new PendingCommit(quorum_size, msg.tid());

        req->primary_shard = msg.primaryshard();
        req->primary_coordinator = msg.primarycoordinator();
        for(uint64_t p : msg.participants()){
            req->participants.push_back(p);
        }
        req->timestamp = msg.timestamp();

        pendingCommits[msg.tid()] = req;
    } else {
        req = dynamic_cast<PendingCommit *>(it->second);
        if(req->participants.size() == 0){
            for(uint64_t p : msg.participants()){
                req->participants.push_back(p);
            }
        }
        req->primary_shard = msg.primaryshard();
        req->primary_coordinator = coordinator_id;
        if(msg.timestamp() > req->timestamp)
            req->timestamp = msg.timestamp();
    }
    ASSERT(req != nullptr);

    Debug("participants number: %lu", req->participants.size());

    // TODO send replicate reply to correspondent coordinator

    if(msg.vote() == proto::CommitResult::PREPARED){
        req->votes[msg.shard()] =  msg.vote();
        if(ReachConsensus(req)){
            req->state = proto::CommitResult::PREPARED;
            req->reach_consensus = true;

            NotifyCommitDecision(req);

            if(req->primary_coordinator == coordinator_id && req->reach_fault_tolerance){
                commitResults[msg.tid()] = new CommitState(req->state, true);
                NotifyReplicateResult(req);

                pendingCommits.erase(msg.tid());
                delete req;
            }
        }
    } else {
        req->state = proto::CommitResult::ABORT;
        req->reach_consensus = true;
        commitResults[msg.tid()] = new CommitState(proto::CommitResult::ABORT, false);

        NotifyCommitDecision(req);

        pendingCommits.erase(msg.tid());
        delete req;

        return;

    }


    // Transfer the replicate reply of a follower to the correspondent coordinator.
    proto::BypassLeaderReply bypassLeaderReply;

    Debug("Send the replicate reply of shard %lu replica %lu to correspondent coordinator", msg.shard(), msg.replica());

    bypassLeaderReply.set_tid(msg.tid());
    bypassLeaderReply.set_shard(msg.shard());
    bypassLeaderReply.set_replica(msg.replica());

    transport->SendMessageToReplica(this, msg.primarycoordinator(), bypassLeaderReply);

    // req->colocatedReplicateReplies.insert(msg.shard());
    // if(req->colocatedReplicateReplies.size() == msg.participants().size()){
    //     proto::BypassLeaderReplyUnion bypassLeaderReplyUnion;

    //     bypassLeaderReplyUnion.set_tid(msg.tid());
    //     bypassLeaderReplyUnion.set_replica(msg.replica());

    //     for(auto p : msg.participants()){
    //         bypassLeaderReplyUnion.add_shards(p);
    //     }
    //     transport->SendMessageToReplica(this, msg.primarycoordinator(), bypassLeaderReplyUnion);
    // }

    
}

void 
CoordinatorReplica::HandleBypassLeaderReply(const TransportAddress &remote,
                                const proto::BypassLeaderReply &msg)
{
    Debug("Receive the replicate reply of txn %lu from shard %lu replica %lu", msg.tid(), 
        msg.shard(), msg.replica());
    
    // transaction has already been made the decision.
    if(commitResults.find(msg.tid()) != commitResults.end()){
        Debug("Transaction %lu has already been made the decision, ignore the request.", msg.tid());
        return;
    }

    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        // auto timer = std::unique_ptr<Timeout>(new Timeout(
        //     transport, 2000, [this]() {  }));

        req = new PendingCommit(quorum_size, msg.tid());

        pendingCommits[msg.tid()] = req;
    } else {
        req = dynamic_cast<PendingCommit *>(it->second);
    }
    ASSERT(req != nullptr);

    bool already_received = false;
    for(auto p : req->replicateResults[msg.shard()]){
        if(p.replica() == msg.replica()){
            already_received = true;
            break;
        }
    }

    if(!already_received){
        req->replicateResults[msg.shard()].push_back(msg);
        if(ReachFaultTolerance(req)){
            req->reach_fault_tolerance = true;
            if(req->reach_consensus){
                commitResults[msg.tid()] = new CommitState(proto::CommitResult::COMMIT, true);
                NotifyReplicateResult(req);

                pendingCommits.erase(msg.tid());
                delete req;
            }
        }
    }
}

void 
CoordinatorReplica::HandleBypassLeaderReplyUnion(const TransportAddress &remote,
                                const proto::BypassLeaderReplyUnion &msg)
{
    Debug("Receive the replicate reply union of txn %lu from replica %lu", msg.tid(), msg.replica());
    
    // transaction has already been made the decision.
    if(commitResults.find(msg.tid()) != commitResults.end()){
        Debug("Transaction %lu has already been made the decision, ignore the request.", msg.tid());
        return;
    }

    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        // auto timer = std::unique_ptr<Timeout>(new Timeout(
        //     transport, 2000, [this]() {  }));

        req = new PendingCommit(quorum_size, msg.tid());

        pendingCommits[msg.tid()] = req;
    } else {
        req = dynamic_cast<PendingCommit *>(it->second);
    }
    ASSERT(req != nullptr);

    bool already_received = false;
    for(auto q : msg.shards()){
    for(auto p : req->replicateResults[q]){
        if(p.replica() == msg.replica()){
            already_received = true;
            break;
        }
    }

    if(!already_received){
        proto::BypassLeaderReply reply;
        reply.set_tid(msg.tid());
        reply.set_replica(msg.replica());
        reply.set_shard(q);
        req->replicateResults[q].push_back(reply);

        if(ReachFaultTolerance(req)){
            req->reach_fault_tolerance = true;
            if(req->reach_consensus){
                commitResults[msg.tid()] = new CommitState(proto::CommitResult::COMMIT, true);
                NotifyReplicateResult(req);
                pendingCommits.erase(msg.tid());
                delete req;
                return;
            }
        }
    }
    }
}

// coordinator handles the commit decision from the correspondent coordinator
void 
CoordinatorReplica::HandleReplicateDecision(const TransportAddress &remote,
                                const proto::ReplicateDecision &msg)
{
    Debug("Receive the commit decision of txn %lu", msg.tid());

    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        Debug("Has not received the transaction");
        return;
    }

    req = dynamic_cast<PendingCommit *>(it->second);

    req->state = msg.state();

    proto::ReplyDecision reqMsg;
    reqMsg.set_tid(msg.tid());
    reqMsg.set_timestamp(req->timestamp);
    reqMsg.set_coordinatorid(coordinator_id);

    if(req->state == proto::CommitResult::COMMIT){
        commitResults[msg.tid()] = new CommitState(proto::CommitResult::COMMIT, false);

        int primaryCoordinator = req->primary_shard % coordinator_config.n;
        transport->SendMessageToReplica(this, primaryCoordinator, reqMsg);

        if(primaryCoordinator != coordinator_id){
            pendingCommits.erase(msg.tid());
            delete req;
        }
    } else {
        commitResults[msg.tid()] = new CommitState(proto::CommitResult::ABORT, false);

        int primaryCoordinator = req->primary_shard % coordinator_config.n;
        transport->SendMessageToReplica(this, primaryCoordinator, reqMsg);

        if(primaryCoordinator != coordinator_id){
            pendingCommits.erase(msg.tid());
            delete req;
        }
    }
}

void 
CoordinatorReplica::HandleReplyDecision(const TransportAddress &remote,
                            const proto::ReplyDecision &msg)
{
    Debug("Receive the reply of commit decision of txn %lu from coordinator %lu", msg.tid(), msg.coordinatorid());

    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        Debug("Has not received the transaction");
        return;
    }

    req = dynamic_cast<PendingCommit *>(it->second);


    if(req->commitDecisionReplies.find(msg.coordinatorid()) == req->commitDecisionReplies.end()){
        req->commitDecisionReplies.insert(msg.coordinatorid());
        if(req->commitDecisionReplies.size() >= req->majority){
            pendingCommits.erase(msg.tid());
            delete req;
        }
    }
}

// check if all participants decide to commit the transaction
bool 
CoordinatorReplica::ReachConsensus(PendingCommit *req){
    Debug("Check if reach consensus");
    Debug("participants: %lu", req->participants.size());
    Debug("votes: %lu", req->votes.size());
    if(req->votes.size() != req->participants.size())
        return false;
    
    for(uint64_t p : req->participants){
        if(req->votes[p] != proto::CommitResult::PREPARED){
            return false;
        }
    }
    Debug("Reach consensus");
    return true;
}

// check if all participants has replicate the transaction to a majority
bool 
CoordinatorReplica::ReachFaultTolerance(PendingCommit *req){
    Debug("Check if txn log has been stored on the majority");
    if(req->participants.size()<=0)
        return false;

    if(req->replicateResults.size() != req->participants.size())
        return false;
    
    for(auto p : req->replicateResults){
        if(p.second.size() < req->majority)
            return false;
    }

    Debug("Reach fault tolerance");

    return true;
}


void
CoordinatorReplica::HandleNotifyDecision(const TransportAddress &remote,
                                         const proto::NotifyDecisionToPrimary &msg){
    Debug("Receive the notify decision of %lu from coordinator replicas", msg.tid());
    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        Debug("Has not received the transaction");
        return;
    }

    req = dynamic_cast<PendingCommit *>(it->second);

    if(req->reach_consensus)
        return;

    req->reach_consensus = true;
    req->state = msg.state();
    if(req->participants.size() == 0){
        req->primary_shard = msg.primaryshard();
        for(uint64_t p : msg.participants()){
            req->participants.push_back(p);
        }
    }

    if(req->state == proto::CommitResult::COMMIT){
        req->timestamp = msg.timestamp();
        if(req->reach_fault_tolerance){
            commitResults[msg.tid()] = new CommitState(req->state, true);
            NotifyReplicateResult(req);

            pendingCommits.erase(msg.tid());
            delete req;
        }
    } else {
        commitResults[msg.tid()] = new CommitState(proto::CommitResult::ABORT, false);

        NotifyCommitDecision(req);

        pendingCommits.erase(msg.tid());
        delete req;
    }
}





// Coordinator notify the co-located leaders the pre-commit decision
void 
CoordinatorReplica::NotifyCommitDecision(PendingCommit *req){
    Debug("Transaction %lu has reach a consensus on commit, notify the closest leader.", req->tid);
    proto::NotifyDecision reqMsg;
    reqMsg.set_tid(req->tid);
    reqMsg.set_state(req->state);
    reqMsg.set_timestamp(req->timestamp);

    // proto::NotifyDecisionToPrimary notDecToPrimary;
    // notDecToPrimary.set_tid(req->tid);
    // notDecToPrimary.set_state(req->state);
    // notDecToPrimary.set_primaryshard(req->primary_shard);
    // notDecToPrimary.set_timestamp(req->timestamp);

    Debug("coor id %lu", coordinator_id);

    // std::vector<TransportAddress*> addrs = closestLeadersList[req->tid];
    // for(TransportAddress* a : addrs){
    //     if(!(transport->SendMessage(this, *a, reqMsg))){
    //         Warning("Notify closest leaders the commit decision failed.");
    //     }
    // }

    // Notify closest participant leaders the decision
    for(auto p : req->participants){
        Debug("participant id: %lu", p);
        if(p % coordinator_config.n == coordinator_id) {
            Debug("Send decision to shard %lu", p);
            Debug("shards nnumber %lu", shardTransport.size());
            if(!(shardTransport[(int)p]->SendMessageToReplica(this, coordinator_id, reqMsg))){
                Warning("Notify closest leaders the commit decision failed.");
            }           
        }
        // notDecToPrimary.add_participants(p);
    }
    
    // // Notify primary coordinator the decision 
    // int primaryCoordinator = req->primary_shard % coordinator_config.n;
    // transport->SendMessageToReplica(this, primaryCoordinator, notDecToPrimary);
}



// Correspondent coordinator send commit decision to primary shard leader (the leader co-locates with the client)
void
CoordinatorReplica::NotifyReplicateResult(PendingCommit *req){
    Debug("Transaction %lu has reached COMMIT", req->tid);
    proto::NotifyReplicateResult reqMsg;
    reqMsg.set_tid(req->tid);
    reqMsg.set_replicatesuccess(true);
    reqMsg.set_timestamp(req->timestamp);

    int primaryShard = req->primary_shard;

    if(!(shardTransport[primaryShard]->SendMessageToReplica(this, coordinator_id, reqMsg))){
        Warning("Notify the primary leader the replicate result failed.");
    }

    proto::ReplicateDecision replicateDecision;
    replicateDecision.set_tid(req->tid);
    replicateDecision.set_state(req->state);
    replicateDecision.set_timestamp(req->timestamp);

    if(!(transport->SendMessageToAll(this, replicateDecision))){
        Warning("Notify other coordinators the commit decision failed.");
    }
}

void
CoordinatorReplica::HandleRequestCommitResult(
    const TransportAddress &remote,
    const proto::RequestCommitResult &msg)
{
    Debug("Received the request of commit result of txn %lu from shard %Lu",
            msg.tid(), msg.shard());
    
    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        req = new PendingCommit(quorum_size, msg.tid());

        req->primary_shard = msg.primaryshard();
        req->primary_coordinator = coordinator_id;
        for(uint64_t p : msg.participants()){
            req->participants.push_back(p);
        }

        // transaction has already been made the decision.
        if(commitResults.find(msg.tid()) != commitResults.end()){
            NotifyReplicateResult(req);
            Debug("Transaction %lu has already been made the decision, ignore the request.", msg.tid());
            delete req;
            return;
        }

        pendingCommits[msg.tid()] = req;
    } else {
        req = dynamic_cast<PendingCommit *>(it->second);
        if(req->participants.size() == 0){
            for(uint64_t p : msg.participants()){
                req->participants.push_back(p);
            }
        }
        req->primary_shard = msg.primaryshard();
        Debug("Coordinator %lu has received txn %lu, waits for its result", 
                coordinator_id, msg.tid());
        return;
    }
}


void
CoordinatorReplica::HandleVote(const TransportAddress &remote,
                    const proto::Vote &msg){
    Debug("Received the vote of txn %lu from shard %lu, vote: %d", msg.tid(), msg.shard(), msg.vote());
    
    // transaction has already been made the decision.
    if(commitResults.find(msg.tid()) != commitResults.end()){
        Debug("Transaction %lu has already been made the decision, ignore the request.", msg.tid());
        return;
    }

    PendingCommit *req = NULL;
    auto it = pendingCommits.find(msg.tid());
    if(it == pendingCommits.end()){
        // auto timer = std::unique_ptr<Timeout>(new Timeout(
        //     transport, 2000, [this]() {  }));

        req = new PendingCommit(quorum_size, msg.tid());

        req->primary_shard = msg.primaryshard();
        req->primary_coordinator = msg.primarycoordinator();
        for(uint64_t p : msg.participants()){
            req->participants.push_back(p);
        }
        req->timestamp = msg.timestamp();

        pendingCommits[msg.tid()] = req;
    } else {
        req = dynamic_cast<PendingCommit *>(it->second);
        if(req->participants.size() == 0){
            for(uint64_t p : msg.participants()){
                req->participants.push_back(p);
            }
        }
        req->primary_shard = msg.primaryshard();
        req->primary_coordinator = coordinator_id;
        if(msg.timestamp() > req->timestamp)
            req->timestamp = msg.timestamp();
    }
    ASSERT(req != nullptr);

    Debug("participants number: %lu", req->participants.size());

    // if(msg.shard() == req->primary_shard){
    //     primaryShardLeaderList[msg.tid()] = remote.clone();
    // }

    // if(msg.replica() == coordinator_id){
    //     closestLeadersList[msg.tid()].push_back(remote.clone());
    // }

    if(msg.vote() == proto::CommitResult::PREPARED){
        req->votes[msg.shard()] =  msg.vote();
        if(ReachConsensus(req)){
            req->state = proto::CommitResult::PREPARED;
            req->reach_consensus = true;

            NotifyCommitDecision(req);

            if(req->primary_coordinator == coordinator_id && req->reach_fault_tolerance){
                commitResults[msg.tid()] = new CommitState(req->state, true);
                NotifyReplicateResult(req);

                pendingCommits.erase(msg.tid());
                delete req;

                // TransportAddress *addr = primaryShardLeaderList[msg.tid()];
                // primaryShardLeaderList.erase(msg.tid());
                // if(addr != NULL)
                //     delete addr;
            }
        }
    } else {
        req->state = proto::CommitResult::ABORT;
        req->reach_consensus = true;
        commitResults[msg.tid()] = new CommitState(proto::CommitResult::ABORT, false);

        NotifyCommitDecision(req);

        pendingCommits.erase(msg.tid());
        delete req;

        // TransportAddress *addr = primaryShardLeaderList[msg.tid()];
        // primaryShardLeaderList.erase(msg.tid());
        // if(addr != NULL)
        //     delete addr;
        
        // std::vector<TransportAddress*> addrs = closestLeadersList[msg.tid()];
        // closestLeadersList.erase(msg.tid());
        // for(TransportAddress * a : addrs){
        //     delete a;
        // }
    }
}


// Primary coordinator check if all participant shards have recived replicate reply from a majority.
// void
// CoordinatorReplica::HandleReplicateResult(const TransportAddress &remote,
//                                          const proto::ReplicateResult &msg){
//     Debug("Receive the replicate result of txn %lu on shard %lu from replica %lu", 
//             msg.tid(), msg.shard(), msg.replica());
    
//     // transaction has already been made the decision.
//     if(commitResults.find(msg.tid()) != commitResults.end()){
//         Debug("Transaction %lu has already been made the decision, ignore the request.", msg.tid());
//         return;
//     }

//     //TODO 同HandleVote，超时怎么处理
//     PendingCommit *req = NULL;
//     auto it = pendingCommits.find(msg.tid());
//     if(it == pendingCommits.end()){
//         // auto timer = std::unique_ptr<Timeout>(new Timeout(
//         //     transport, 2000, [this]() {  }));

//         req = new PendingCommit(quorum_size, msg.tid());

//         // req->primary_shard = msg.primaryshard();
//         // req->primary_coordinator = coordinator_id;
//         // for(uint64_t p : msg.participants()){
//         //     req->participants.push_back(p);
//         // }

//         pendingCommits[msg.tid()] = req;
//     } else {
//         req = dynamic_cast<PendingCommit *>(it->second);
//     }
//     ASSERT(req != nullptr);

//     bool already_received = false;
//     for(auto p : req->replicateResults[msg.shard()]){
//         if(p.replica() == msg.replica()){
//             already_received = true;
//             break;
//         }
//     }

//     if(!already_received){
//         req->replicateResults[msg.shard()].push_back(msg);
//         if(ReachFaultTolerance(req)){
//             req->reach_fault_tolerance = true;
//             if(req->reach_consensus){
//                 commitResults[msg.tid()] = new CommitState(proto::CommitResult::COMMIT, true);
//                 NotifyReplicateResult(req);

//                 pendingCommits.erase(msg.tid());
//                 delete req;

//                 // TransportAddress *addr = primaryShardLeaderList[msg.tid()];
//                 // primaryShardLeaderList.erase(msg.tid());
//                 // if(addr != NULL)
//                 //     delete addr;

//                 // std::vector<TransportAddress*> addrs = closestLeadersList[msg.tid()];
//                 // closestLeadersList.erase(msg.tid());
//                 // for(TransportAddress * a : addrs){
//                 //     delete a;
//                 // }
//             }
//         }
//     }
// }


}
}