

#include "store/strongstore/server.h"

namespace strongstore {

using namespace std;
using namespace proto;

Server::Server(Mode mode, TpcMode tpcMode, uint64_t skew, uint64_t error) : mode(mode), tpcMode(tpcMode)
{
    timeServer = TrueTime(skew, error);

    switch (mode) {
    case MODE_LOCK:
    case MODE_SPAN_LOCK:
        Debug("LOCK STORE");
        store = new strongstore::LockStore();
        break;
    case MODE_OCC:
    case MODE_SPAN_OCC:
        store = new strongstore::OCCStore();
        break;
    default:
        NOT_REACHABLE();
    }
}

Server::~Server()
{
    delete store;
}

void
Server::GetUpcall(const string &str1, string &str2){
    Request request;
    Reply reply;
    int status;
    uint64_t dependency = 0;

    request.ParseFromString(str1);

    Debug("Received GetUpcall, key: %s", request.get().key().c_str());

    if (request.get().has_timestamp()) {
        pair<Timestamp, string> val;
        
        status = store->Get(request.txnid(), request.get().key(),
                               request.get().timestamp(), val, dependency);
        if (status == 0) {
            reply.set_value(val.second);
        } else {
            Debug("Get data failed");
        }
    } else {
        pair<Timestamp, string> val;
        status = store->Get(request.txnid(), request.get().key(), val, dependency);
        if (status == 0) {
            reply.set_value(val.second);
            reply.set_timestamp(val.first.getTimestamp());
        } else {
            Debug("Get data failed");
        }
    }
    if(tpcMode == TpcMode::MODE_PARALLEL){
        reply.set_dependency(dependency);
    }
    reply.set_status(status);
    reply.SerializeToString(&str2);
}

void 
Server::PrepareUpcall(const string &str1, string &str2, bool &replicate, bool do_check){
    Request request;
    Reply reply;
    int status;

    request.ParseFromString(str1);

    Debug("Receive PrepareUpcall of txn %lu", request.txnid());
    status = store->Prepare(request.txnid(), Transaction(request.prepare().txn()), do_check);

    // if prepared, then replicate result
    if (status == REPLY_OK || status == REPLY_HAS_PRECOMMIT) {
        replicate = true;
        // // get a prepare timestamp and send along to replicas
        // if (mode == MODE_SPAN_LOCK || mode == MODE_SPAN_OCC) {
        //     request.mutable_prepare()->set_timestamp(timeServer.GetTime());
        // }
        reply.set_status(status);
        reply.SerializeToString(&str2);
    } else {
        // if abort, don't replicate
        replicate = false;
        reply.set_status(status);
        reply.SerializeToString(&str2);
    }
}

void
Server::CommitUpcall(const string &str1, string &str2){
    Request request;
    Reply reply;
    int status = 0;
    
    request.ParseFromString(str1);
     
    std::vector<uint64_t> nexts = store->Commit(request.txnid(), request.commit().timestamp());
    reply.set_status(status);

    for(auto next_txn : nexts){
        reply.add_nexts(next_txn);
    }

    reply.SerializeToString(&str2);
}

void
Server::AbortUpcall(const string &str1, string &str2){
    Request request;
    Reply reply;
    int status = 0;

    request.ParseFromString(str1);

     std::vector<uint64_t> nexts = store->Abort(request.txnid(), Transaction(request.abort().txn()));

    reply.set_status(status);
    
    for(auto next_txn : nexts){
        reply.add_nexts(next_txn);
    }
    
    reply.SerializeToString(&str2);
}

void
Server::PreCommitUpcall(const string &str1, string &str2){
    Request request;
    Reply reply;
    int status = 0;

    request.ParseFromString(str1);

    store->PreCommit(request.txnid());

    reply.set_status(status);
    reply.SerializeToString(&str2);
}


void
Server::Load(const string &key, const string &value, const Timestamp timestamp)
{
    store->Load(key, value, timestamp);
}

} // namespace strongstore

int
main(int argc, char **argv)
{
    int index = -1;
    unsigned int myShard=0, maxShard=1, nKeys=1;
    unsigned int maxReplica=1;
    const char *configPath = NULL;
    string coordinatorConfigPath;
    // const char *leaderConfigPath = NULL;
    const char *keyPath = NULL;
    int64_t skew = 0, error = 0;
    Mode mode;
    TpcMode tpcMode;

    // Parse arguments
    int opt;
    while ((opt = getopt(argc, argv, "c:i:m:e:s:f:n:S:N:k:t:")) != -1) {
        switch (opt) {
        case 'c':
            configPath = optarg;
            break;
            
        case 'i':
        {
            char *strtolPtr;
            index = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (index < 0))
            {
                fprintf(stderr, "option -i requires a numeric arg\n");
            }
            break;
        }
        
        case 'm':
        {
            if (strcasecmp(optarg, "lock") == 0) {
                mode = MODE_LOCK;
            } else if (strcasecmp(optarg, "occ") == 0) {
                mode = MODE_OCC;
            } else if (strcasecmp(optarg, "span-lock") == 0) {
                mode = MODE_SPAN_LOCK;
            } else if (strcasecmp(optarg, "span-occ") == 0) {
                mode = MODE_SPAN_OCC;
            } else {
                fprintf(stderr, "unknown mode '%s'\n", optarg);
            }
            break;
        }

        case 's':
        {
            char *strtolPtr;
            skew = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (skew < 0))
            {
                fprintf(stderr, "option -s requires a numeric arg\n");
            }
            break;
        }

        case 'e':
        {
            char *strtolPtr;
            error = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (error < 0))
            {
                fprintf(stderr, "option -e requires a numeric arg\n");
            }
            break;
        }

        case 'k':
        {
            char *strtolPtr;
            nKeys = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr, "option -k requires a numeric arg\n");
            }
            break;
        }

        case 'n':
        {
            char *strtolPtr;
            myShard = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr, "option -n requires a numeric arg\n");
            }
            break;
        }

        case 'S':
        {
            char *strtolPtr;
            maxShard = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr, "option -S requires a numeric arg\n");
            }
            break;
        }

        case 'N':
        {
            char *strtolPtr;
            maxReplica = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (maxReplica <= 0))
            {
                fprintf(stderr, "option -N requires a numeric arg\n");
            }
            break;
        }

        case 'f':   // Load keys from file
        {
            keyPath = optarg;
            break;
        }

        case 't':
        {
            if (strcasecmp(optarg, "fast") == 0) {
                tpcMode = MODE_FAST;
            } else if (strcasecmp(optarg, "slow") == 0) {
                tpcMode = MODE_SLOW;
            } else if (strcasecmp(optarg, "parallel") == 0) {
                tpcMode = MODE_PARALLEL;
            } else if (strcasecmp(optarg, "carousel") == 0) {
                tpcMode = MODE_CAROUSEL;
            } else if (strcasecmp(optarg, "rc") == 0) {
                tpcMode = MODE_RC;
            } else {
                fprintf(stderr, "unknown tpcMode '%s'\n", optarg);
            }
            break;
        }

        default:
            fprintf(stderr, "Unknown argument %s\n", argv[optind]);
        }


    }

    if (!configPath) {
        fprintf(stderr, "option -c is required\n");
    }

    if (index == -1) {
        fprintf(stderr, "option -i is required\n");
    }

    if (mode == MODE_UNKNOWN) {
        fprintf(stderr, "option -m is required\n");
    }



    // Load configuration
    std::ifstream configStream(configPath);
    if (configStream.fail()) {
        fprintf(stderr, "unable to read configuration file: %s\n", configPath);
    }
    transport::Configuration config(configStream);

    if (index >= config.n) {
        fprintf(stderr, "replica index %d is out of bounds; "
                "only %d replicas defined\n", index, config.n);
    }

    UDPTransport transport(0.0, 0.0, 0);

    // Load coordinator replicas configuration
    std::string str(configPath);
    std::string config_prefix = str.substr(0, str.length()-8);
    coordinatorConfigPath = config_prefix + ".coor.config";
    fprintf(stderr, "coordinator config path: %s\n", coordinatorConfigPath.c_str());
    std::ifstream coordinatorConfigStream(coordinatorConfigPath);
    if (coordinatorConfigStream.fail()) {
        fprintf(stderr, "unable to read coordinator configuration file: %s\n", coordinatorConfigPath.c_str());
    }
    transport::Configuration coordinatorConfig(coordinatorConfigStream);

    UDPTransport coordinator_transport(0.0, 0.0, 0);

    std::vector<transport::Configuration> shardConfig;
    std::vector<Transport *> shardTransport;

    for(int i = 0; i < maxShard; i++){
        std::string cpath = config_prefix + std::to_string(i) + ".config";
        fprintf(stderr, "shard %d config path: %s\n", i, cpath.c_str());

        std::ifstream cStream(cpath);
        if (cStream.fail()) {
            fprintf(stderr, "unable to read shard %d configuration file: %s\n", i, cpath.c_str());
        }
        shardConfig.push_back(transport::Configuration(cStream));
        shardTransport.push_back(new UDPTransport(0.0, 0.0, 0));
    }

    strongstore::Server server(mode, tpcMode, skew, error);
    replication::lr::LRReplica replica(shardConfig, config, coordinatorConfig, index, 
                    (myShard % config.n), myShard, maxShard, shardTransport, &transport, 
                    &coordinator_transport, &server, mode, tpcMode);
    Debug("Number of shards: %d, myShard: %d", maxShard, myShard);
    if (keyPath) {
        string key;
        std::ifstream in;
        in.open(keyPath);
        Debug("Key path: %s", keyPath);
        if (!in) {
            fprintf(stderr, "Could not read keys from: %s\n", keyPath);
            exit(0);
        }

        for (unsigned int i = 0; i < nKeys; i++) {
            getline(in, key);
            
            uint64_t hash = 5381;
            const char* str = key.c_str();
            for (unsigned int j = 0; j < key.length(); j++) {
                hash = ((hash << 5) + hash) + (uint64_t)str[j];
            }
            // Debug("Number of shards: %d, myShard: %d", maxShard, myShard);
            if (hash % maxShard == myShard) {
                // Debug("Load key:%s", key.c_str());
                server.Load(key, "null", Timestamp());
            }
        }
        in.close();
    }

    transport.Run();

    return 0;
}
