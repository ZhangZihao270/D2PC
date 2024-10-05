#ifndef _LR_REPLICA_H_
#define _LR_REPLICA_H_

#include <sys/time.h>
#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/persistent_register.h"
#include "lib/udptransport.h"
#include "replication/common/quorumset.h"
#include "replication/common/replica.h"
#include "replication/lr/lr-proto.pb.h"
#include "replication/lr/log.h"
#include "store/common/transaction.h"
#include "store/strongstore/strong-proto.pb.h"
// #include "store/strongstore/server.h"
#include "replication/commit/commit-proto.pb.h"
#include "replication/commit/coordinator.h"

using namespace replication::commit;

namespace replication{
namespace lr{


// enum Mode {
//     MODE_UNKNOWN,
//     MODE_OCC,
//     MODE_LOCK
// };

class LRAppReplica
{
public:
    LRAppReplica(){};
    virtual ~LRAppReplica(){};
    virtual void GetUpcall(const string &str1, string &str2) {};
    virtual void PrepareUpcall(const string &str1, string &str2, bool &replicate, bool isLeader){};
    virtual void CommitUpcall(const string &str1, string &str2){};
    virtual void AbortUpcall(const string &str1, string &str2){};
    virtual void PreCommitUpcall(const string &str1, string &str2){};
};

class LRReplica : TransportReceiver{
public:
    LRReplica(std::vector<transport::Configuration> config, transport::Configuration replica_config,
            transport::Configuration coordinator_config, 
            uint64_t replica_id, uint64_t leader, uint64_t shard, int nshards,
            std::vector<Transport *> shardTransport, Transport *transport,
            Transport *coordinator_transport, LRAppReplica *app, Mode mode, TpcMode tpcMode);

    ~LRReplica();

    void ReceiveMessage(const TransportAddress &remote,
                        const std::string &type, const std::string &data);
    void HandleMessage(const TransportAddress &remote,
                       const std::string &type, const std::string &data);

    void HandlePrepareTxn(const TransportAddress &remote,
                        const proto::PrepareTransaction &msg);
    void HandleReplicateTransaction(const TransportAddress &remote,
                                    const proto::ReplicateTransaction &msg);
    void HandleReplicateTransactionReply(const TransportAddress &remote,
                                        const proto::ReplicateTransactionReply &msg);

    void HandleCommit(const proto::Commit &msg);
    void HandleReplicateCommit(const TransportAddress &remote,
                    const proto::ReplicateCommit &msg);
    void HandleReplicateCommitReply(const TransportAddress &remote,
                    const proto::ReplicateCommitReply &msg);
    
    void HandleParallelModeCommit(const TransportAddress &remote,
                                const proto::ParallelModeCommit &msg);
    
    // slow mode
    void HandlePrepare(const TransportAddress &remote,
                        const proto::PrepareTransaction &msg);
    void HandlePrepareTransactionReply(const TransportAddress &remote,
                                        const proto::PrepareTransactionReply &msg);
    

    // handle messages from coordinator replica
    void HandleDecisionFromCoordinatorReplica(const TransportAddress &remote,
                                            const replication::commit::proto::NotifyDecision &msg);
    void HandleReplicateResultFromCoordinatorReplica(const TransportAddress &remote,
                                                    const replication::commit::proto::NotifyReplicateResult &msg);

    void HandleRead(const TransportAddress &remote,
                        const proto::GetData &msg);

    void HandleReadAndPrepare();

    void PreCommit(const uint64_t tid);
private:
    // used by leader to track the transaction it replicates to others
    struct PendingTransaction{
        // LogEntry txnlog;
        uint64_t lsn;
        uint64_t tid;
        QuorumSet<uint64_t, proto::ReplicateTransactionReply> quorum;
        std::size_t majority;
        proto::LogEntryState finalResult;
        // bool continuationInvoked = false;
        std::unique_ptr<Timeout> timer;
        std::vector<uint64_t> participants;
        uint64_t primary_shard;

        bool send_confirms = false;
        bool reach_consensus = false; 
        bool reach_commit_decision = false;

        bool parallel_mode = false;

        bool is_participant = true;
        
        bool has_precommit = false;

        inline PendingTransaction(
            std::unique_ptr<Timeout> timer,
            int majority,
            uint64_t lsn,
            uint64_t tid,
            std::vector<uint64_t> participants,
            uint64_t primary_shard
        ) : timer(std::move(timer)),
            lsn(lsn),
            tid(tid),
            quorum(majority),
            majority(majority),
            participants(participants),
            primary_shard(primary_shard){};

        inline PendingTransaction(bool parallel_mode, int majority) 
            : parallel_mode(parallel_mode), quorum(majority), majority(majority) {};
        
        virtual ~PendingTransaction(){};
    };
    
    struct PendingPrepare{
        uint64_t tid;
        uint64_t timestamp = 0;
        Transaction txn;
        std::unordered_map<uint64_t, proto::LogEntryState> participant_replies;
        std::vector<uint64_t> participants;
        proto::LogEntryState finalDecision;
        std::unique_ptr<Timeout> timer;

        // for RC mode, to collect the decisions from different regions
        std::unordered_map<uint64_t, proto::LogEntryState> region_replicies;

        bool send_confirms = false;
        bool reach_consensus = false;

        inline PendingPrepare(
            std::unique_ptr<Timeout> timer,
            std::vector<uint64_t> participants,
            uint64_t tid,
            Transaction txn
        ) : timer(std::move(timer)),
            tid(tid),
            txn(txn),
            participants(participants){};
        
        virtual ~PendingPrepare(){};
    };

    uint64_t increaseTs(){
        return ++ current_ts;
    }

    uint64_t getCurrentTs(){
        return current_ts;
    }

    uint64_t newTxnId(){
        return ++ last_transaction;
    }

    void InvokeReplicateTxn(const LogEntry *entry, bool canvote);
    void ReplicateTxn(const PendingTransaction *req);
    void HandleConsensus(const uint64_t txn_id,
                        const std::map<int, proto::ReplicateTransactionReply> &msgs,
                        PendingTransaction *req);
    void SendPrepare(const PendingPrepare *req); // slow mode
    
    void HandleParallelCommitResult(uint64_t tid, uint64_t timestamp, proto::LogEntryState res);

    // notify the decision to other replicas
    void notifyCommitToReplica(PendingTransaction *req, const proto::Commit &msg);
    // apply to the store when reaching the commit decision
    void applyToStore(PendingTransaction *req, const proto::Commit &msg);
    // slow mode, check if receive votes from all participants
    bool ReachFinalDecision(PendingPrepare *req);
    void ReplyFinalDecision(PendingPrepare *req);
    // RC mode
    bool ReachConsensusOnFinalDecision(PendingPrepare *req);

    void ResendReplicateTxn(uint64_t tid);
    void ResendPrepareOK(uint64_t tid);
    void ResendPrepare(uint64_t tid);

    void CascadeAbort(uint64_t tid);

    void VoteForNext(std::vector<uint64_t> nexts);
    void notifyForFollowingDependenies(uint64_t tid);

    // Sharding logic: Given key, generates a number b/w 0 to nshards-1
    uint64_t key_to_shard(const std::string &key) {
        uint64_t hash = 5381;
        const char* str = key.c_str();
        for (unsigned int i = 0; i < key.length(); i++) {
            hash = ((hash << 5) + hash) + (uint64_t)str[i];
        }

        return (hash % nshards);
    }

    transport::Configuration replica_config;
    transport::Configuration coordinator_config;
    uint64_t replica_id;
    uint64_t shard_id;
    uint64_t leader;
    int nshards;


    Transport *transport;
    Transport *coordinator_transport;
    LRAppReplica *store;

    std::vector<transport::Configuration> config;
    std::vector<Transport *> shardTransport;

    // concurrency mode
    Mode mode;
    // two phase commit mode: fast/slow/parallel
    TpcMode tpcMode;
    bool isLeader = false;

    term_t term;

    Log log;
    // Lsn commit_index;
    // Lsn apply_index;

    uint64_t current_ts;
    uint64_t last_transaction;

    std::unordered_map<txn_id, PendingTransaction *> pendingTxns; // store txns to be replicated
    std::unordered_map<txn_id, PendingPrepare *> pendingPrepares; // store prepare requests
    std::unordered_map<txn_id, Transaction *> activeTxns;
    std::unordered_map<txn_id, std::pair<proto::LogEntryState, uint64_t>> committedTxns;

    std::unordered_map<txn_id, int> inDependency;

    std::unordered_map<txn_id, TransportAddress*> clientList;
    std::unordered_map<txn_id, TransportAddress*> prepareClientList; // for slow mode
    // // store the shard id and replica id of the coordinator
    // std::unordered_map<txn_id, std::pair<uint64_t, uint64_t>> coordinatorList;
};

}
}
#endif