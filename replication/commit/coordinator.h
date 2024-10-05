#ifndef _COMMIT_COORDINATOR_H_
#define _COMMIT_COORDINATOR_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/persistent_register.h"
#include "lib/udptransport.h"
#include "replication/commit/commit-proto.pb.h"

namespace replication{
namespace commit{

struct CommitState{
    proto::CommitResult commitState;
    bool reachFaultTolerance = false;

    CommitState(proto::CommitResult commitState, bool reachFaultTolerance) : 
        commitState(commitState), reachFaultTolerance(reachFaultTolerance){};
};

class CoordinatorReplica : TransportReceiver{
public:
    CoordinatorReplica(std::vector<transport::Configuration> config, transport::Configuration coordinator_config,
                    uint64_t coordinator_id, std::vector<Transport *> shardTransport, Transport *transport);

    ~CoordinatorReplica();

    void ReceiveMessage(const TransportAddress &remote,
                        const std::string &type, const std::string &data);
    void HandleMessage(const TransportAddress &remote,
                       const std::string &type, const std::string &data);
    void HandleRequestCommitResult(const TransportAddress &remote,
                        const proto::RequestCommitResult &msg);
    void HandleNotifyDecision(const TransportAddress &remote,
                            const proto::NotifyDecisionToPrimary &msg);
private:
    struct PendingCommit{
        uint64_t tid;
        std::vector<uint64_t> participants;
        // shard_id -> vote of one shard
        std::unordered_map<uint64_t, proto::CommitResult> votes;
        // shard_id -> does txn have dependencies on this shard?
        std::unordered_map<uint64_t, bool> dependencies;
        // shard_id -> replicate result
        std::unordered_map<uint64_t, std::vector<proto::BypassLeaderReply>> replicateResults;
        std::unordered_set<uint64_t> commitDecisionReplies;
        const std::size_t majority;
        // std::unique_ptr<Timeout> timer;
        uint64_t primary_shard;
        uint64_t primary_coordinator;
        uint64_t timestamp = 0;
        proto::CommitResult state;

        std::unordered_set<uint64_t> colocatedReplicateReplies;

        bool reach_consensus = false;
        bool reach_fault_tolerance = false;

        // inline PendingCommit(
        //     std::unique_ptr<Timeout> timer,
        //     int majority,
        //     uint64_t tid
        // ) : timer(std::move(timer)),
        //     majority(majority),
        //     tid(tid){};
        
        inline PendingCommit(
            int majority,
            uint64_t tid
        ) : majority(majority),
            tid(tid){};
    };

    void HandleVote(const TransportAddress &remote,
                    const proto::Vote &msg);
    
    // void HandleReplicateResult(const TransportAddress &remote,
    //                         const proto::ReplicateResult &msg);

    void HandleReplicateReplyAndVote(const TransportAddress &remote,
                                const proto::ReplicateReplyAndVote &msg);

    void HandleBypassLeaderReply(const TransportAddress &remote,
                                const proto::BypassLeaderReply &msg);

    void HandleBypassLeaderReplyUnion(const TransportAddress &remote,
                                const proto::BypassLeaderReplyUnion &msg);

    void HandleReplicateDecision(const TransportAddress &remote,
                                const proto::ReplicateDecision &msg);
    
    void HandleReplyDecision(const TransportAddress &remote,
                            const proto::ReplyDecision &msg);

    void HandleNotifyDependenciesAreCleared(const TransportAddress &remote,
                                const proto::NotifyDependenciesAreCleared &msg);

    // check if has received votes from all participants, and make the decision
    bool ReachConsensus(PendingCommit *req);

    bool ReachFaultTolerance(PendingCommit *req);

    bool AllParticipantsHaveNoPrecommitDependency(PendingCommit *req);

    void NotifyCommitDecision(PendingCommit *req);
    void NotifyReplicateResult(PendingCommit *req);

    transport::Configuration coordinator_config;
    int quorum_size;
    int nreplicas; // number of coordinator replicas
    uint64_t coordinator_id;
    Transport *transport;

    std::vector<transport::Configuration> config;
    std::vector<Transport *> shardTransport;

    std::unordered_map<uint64_t, PendingCommit *> pendingCommits;

    // store the state of transactions that has been committed or aborted
    std::unordered_map<uint64_t, CommitState *> commitResults;

    // std::unordered_map<uint64_t, TransportAddress*> primaryShardLeaderList;
    // std::unordered_map<uint64_t, std::vector<TransportAddress*>> closestLeadersList;
    // std::unordered_map<uint64_t, Vote> votes;
    // std::unordered_map<uint64_t, std::vector<uint64_t>> participants;
};

}
}

#endif