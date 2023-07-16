#ifndef _LR_CLIENT_H_
#define _LR_CLIENT_H_

#include "replication/common/client.h"
#include "replication/common/quorumset.h"
#include "lib/configuration.h"
#include "replication/lr/lr-proto.pb.h"
#include "store/common/transaction.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

using namespace std;
using namespace replication::lr::proto;

namespace replication{
namespace lr{

enum class ErrorCode {
    // For whatever reason (failed replicas, slow network), the request took
    // too long and timed out.
    TIMEOUT,

    // For IR, if a client issues a consensus operation and receives a majority
    // of replies and confirms in different views, then the operation fails.
    MISMATCHED_CONSENSUS_VIEWS
};

class LRClient : public TransportReceiver {
public:
    using get_continuation_t =
        std::function<void(std::vector<std::pair<string, uint64_t>> readKey, std::vector<std::string> values, int state)>;
    using continuation_t =
        std::function<void(const uint64_t tid, const uint64_t shard, int state, uint64_t timestamp)>;
    using parallelcommit_continuation_t = 
        std::function<void(const uint64_t tid, int state, uint64_t timestamp)>;
    // using prepare_continuation_t =
    //     std::function<void(const uint64_t tid, const uint64_t shard, const proto::LogEntryState state)>;
    using error_continuation_t =
        std::function<void(const uint64_t tid, ErrorCode err)>;

    LRClient(const transport::Configuration &config,
             Transport *transport,
             TpcMode tpcMode,
             uint64_t clientid = 0
             );
    virtual ~LRClient();

    // fast mode  client sends prepare to shard leader
    // only contains a piece of transaction
    void InvokePrepare(
        const Transaction &sub_txn,
        uint64_t leader,
        continuation_t continuation,
        error_continuation_t error_continuation = nullptr
    );

    void InvokeGetData(
        uint64_t tid,
        const std::vector<std::string> &keys,
        uint64_t replica_id,
        get_continuation_t continuation,
        uint32_t timeout,
        error_continuation_t error_continuation = nullptr
    );

    void InvokeCommit(
        uint64_t tid,
        uint64_t leader,
        LogEntryState state,
        continuation_t continuation,
        error_continuation_t error_continuation = nullptr
    );

    void InvokeParallelCommit(
        uint64_t tid,
        std::vector<uint64_t> participants,
        uint64_t replica_id,
        parallelcommit_continuation_t continuation,
        error_continuation_t error_continuation = nullptr
    );

    virtual void ReceiveMessage(
        const TransportAddress &remote,
        const string &type,
        const string &data
    ) override;

    // slow mode 

protected:
    struct PendingRequest {
        uint64_t tid;
        // continuation_t continuation;
        bool continuationInvoked = false;
        std::unique_ptr<Timeout> timer;

        inline PendingRequest(uint64_t tid,
                              std::unique_ptr<Timeout> timer)
            : tid(tid),
              timer(std::move(timer)){};
        virtual ~PendingRequest(){};
    };

    struct PendingParallelCommitRequest : public PendingRequest{
        std::vector<uint64_t> participants;
        parallelcommit_continuation_t continuation;
        error_continuation_t error_continuation;

        inline PendingParallelCommitRequest(
            uint64_t tid, std::vector<uint64_t> participants, parallelcommit_continuation_t continuation,
            error_continuation_t error_continuation,
            std::unique_ptr<Timeout> timer)
            : PendingRequest(tid, std::move(timer)),
              participants(participants),
              continuation(continuation),
              error_continuation(error_continuation){};
    };

    struct PendingGetDataRequest : public PendingRequest {
        get_continuation_t continuation;
        error_continuation_t error_continuation;

        inline PendingGetDataRequest(
            uint64_t tid, get_continuation_t continuation,
            error_continuation_t error_continuation,
            std::unique_ptr<Timeout> timer)
            : PendingRequest(tid, std::move(timer)),
              continuation(continuation),
              error_continuation(error_continuation){};
    };

    // struct PendingPrepare : public PendingRequest{
    //     continuation_t continuation;

    //     inline PendingPrepare(uint64_t tid, std::unique_ptr<Timeout> timer, 
    //         continuation_t continuation)
    //         : PendingRequest(tid, std::move(timer)),
    //           continuation(continuation){};
    // }

    // fast mode, each client is responsible for only one shard
    struct PendingPrepare : public PendingRequest {
        Transaction txn;
        int leader;
        continuation_t continuation;
        error_continuation_t error_continuation;
        proto::LogEntryState state;
        uint64_t timestamp = 0;

        // for carousel and RC mode
        std::unordered_map<uint64_t, proto::LogEntryState> replicaReplies;
        bool reachFaultTolerance = false; // leader has replicated to majority

        bool send_confirms = false;
        // bool reach_consensus = false;

        inline PendingPrepare(
            uint64_t tid,
            uint64_t leader,
            std::unique_ptr<Timeout> timer,
            Transaction txn,
            continuation_t continuation,
            error_continuation_t error_continuation
        ) : PendingRequest(tid, std::move(timer)),
            txn(txn), leader(leader),
            continuation(continuation),
            error_continuation(error_continuation){};
        
        virtual ~PendingPrepare(){};
    };

    struct PendingCommitRequest : public PendingRequest {
        continuation_t continuation;
        error_continuation_t error_continuation;

        inline PendingCommitRequest(
            uint64_t tid, continuation_t continuation,
            error_continuation_t error_continuation,
            std::unique_ptr<Timeout> timer)
            : PendingRequest(tid, std::move(timer)),
              continuation(continuation),
              error_continuation(error_continuation){};
    };

    std::unordered_map<uint64_t, PendingRequest *> pendingReqs;
    std::unordered_map<uint64_t, PendingRequest *> pendingParallelCommits;

    transport::Configuration config;
    Transport *transport;

    uint64_t clientid;

    TpcMode tpcMode;

    void SendPrepare(uint64_t leader, const PendingPrepare *req);
    void HandlePrepareTxnReply(const TransportAddress &remote,
                                const PrepareTransactionReply &msg);
    void PrepareTimeoutCallback(const uint64_t tid);


    void HandleGetDataReply(const TransportAddress &remote,
                            const GetDataReply &msg);
    void GetDateTimeoutCallback(const uint64_t tid);

    void HandleParallelModeCommitReply(const TransportAddress &remote,
                            const ParallelModeCommitReply &msg);
    void ParallelModeCommit(const uint64_t tid);

    bool CarouselCheck(PendingPrepare *req);

    bool RCCheck(PendingPrepare *req);
};
}
}

#endif