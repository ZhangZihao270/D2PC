#include "store/strongstore/commitclient.h"

namespace strongstore{

CommitClient::CommitClient(uint64_t client_id, long nshards, std::vector<BufferClient *> bclient, 
                commit_continuation_t commit_continuation)
                : client_id(client_id), nshards(nshards), bclient(bclient), commit_continuation(commit_continuation){};

void
CommitClient::Run(){
    Debug("Start run");
    while(1){
        // Debug("Request size: %lu", requests.size());
        if(requests.size()>0){
            Debug("Receive a prepare request");
            CommitRequest * req = requests.front();
            requests.pop();

            if(req->exit)
                break;

            if(req->mode == 0){
                Debug("Run normal commit");
                Commit(req);
            } else {
                Debug("Run parallel mode commit");
                ParallelModeCommit(req);
            }
        }
    }
}

void 
CommitClient::Commit(CommitRequest * req){
    int status;

    // 1. Send commit-prepare to all shards.
    list<Promise *> promises;
    for (auto p : req->participants) {
        Debug("Sending prepare to shard [%d]", p);
        promises.push_back(new Promise(PREPARE_TIMEOUT));
        bclient[p]->SetParticipants(req->participants);
        bclient[p]->SetPrimaryShard(req->primary_shard);
        bclient[p]->Prepare(Timestamp(),promises.back());
    }

    // 2. Wait for reply from all shards. (abort on timeout)
    Debug("Waiting for PREPARE replies");

    status = REPLY_OK;
    for (auto p : promises) {
        if(p->GetReply() == replication::lr::proto::LogEntryState::LOG_STATE_PREPAREOK || 
            p->GetReply() == replication::lr::proto::LogEntryState::LOG_STATE_COMMIT){
            status = REPLY_OK;
        } else if (p->GetReply() == replication::lr::proto::LogEntryState::LOG_STATE_ABORT){
            status = REPLY_FAIL;
        }
        req->ts = p->GetTimestamp().getTimestamp();
        delete p;
    }

    commit_continuation(status);
}

void
CommitClient::ParallelModeCommit(CommitRequest * req){
    int status;

    Promise *promise = new Promise(PREPARE_TIMEOUT);
    bclient[req->primary_shard]->SetParticipants(req->participants);
    bclient[req->primary_shard]->ParallelModeCommit(promise);

    status = REPLY_OK;
    if(promise->GetReply() == replication::lr::proto::LogEntryState::LOG_STATE_PREPAREOK || 
        promise->GetReply() == replication::lr::proto::LogEntryState::LOG_STATE_COMMIT){
        status = REPLY_OK;
    } else if (promise->GetReply() == replication::lr::proto::LogEntryState::LOG_STATE_ABORT){
        status = REPLY_FAIL;
    }
    delete promise;

    commit_continuation(status);
}
}