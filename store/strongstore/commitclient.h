#ifndef _COMMIT_CLIENT_
#define _COMMIT_CLIENT_

#include "store/strongstore/strong-proto.pb.h"
#include "store/common/frontend/bufferclient.h"
#include "store/strongstore/shardclient.h"
#include <vector>
#include <queue>
#include <set>
#include <list>

using namespace replication::lr;

namespace strongstore{

struct CommitRequest{
    uint64_t tid;
    std::vector<int> participants;
    int primary_shard;
    uint64_t ts;
    int mode; //0 for commit, 1 for parallel commit
    bool exit = false;

    CommitRequest(uint64_t id, std::set<int> participantsets, 
                int primary_shard, uint64_t ts, int mode)
                : tid(id), primary_shard(primary_shard),
                ts(ts), mode(mode){
        for(auto p : participantsets){
            participants.push_back(p);
        }
    }
    CommitRequest(uint64_t id, int primary_shard, int mode)
                : tid(id), primary_shard(primary_shard), mode(mode){};
    CommitRequest(bool exit) : exit(exit){};

};

class CommitClient {
public:
    using commit_continuation_t = 
        std::function<void(int &state)>;

    CommitClient(uint64_t client_id, long nshards, std::vector<BufferClient *> bclient, 
                commit_continuation_t commit_continuation);

    void Run();

    void Commit(CommitRequest * req);
    void ParallelModeCommit(CommitRequest * req);

    std::queue<CommitRequest *> requests;
private:
    uint64_t client_id;

    long nshards;

    std::vector<BufferClient *> bclient;

    commit_continuation_t commit_continuation;
};
}

#endif