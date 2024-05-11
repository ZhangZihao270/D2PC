#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/strongstore/client.h"
// #include "store/weakstore/client.h"
// #include "store/tapirstore/client.h"
#include <algorithm>
#include "tpccTransaction.h"
// #include "config.h"

using namespace std;

int 
main(int argc, char **argv)
{
    const char *configPath = NULL;
    int duration = 10;
    int nShards = 1;
    int nReplicas = 1;
    int closestReplica = -1; // Closest replica id.
    int skew = 0; // difference between real clock and TrueTime
    int error = 0; // error bars
    double dis_ratio = 0;
    // std::vector<vector<queue<uint64_t>> neworders;
    std::vector<std::vector<std::queue<uint64_t>>> neworders(g_ware_num, std::vector<std::queue<uint64_t>>(g_dist_per_ware));



    Client *client;
    enum {
        MODE_UNKNOWN,
        MODE_TAPIR,
        MODE_WEAK,
        MODE_STRONG
    } mode = MODE_UNKNOWN;

    // Mode for strongstore.
    strongstore::Mode strongmode;
    TpcMode tpcMode;

    int opt;
    while ((opt = getopt(argc, argv, "c:d:N:k:f:m:e:s:z:r:n:t:D:")) != -1) {
        switch (opt) {
        case 'c': // Configuration path
        { 
            configPath = optarg;
            break;
        }

        // case 'f': // Generated keys path
        // { 
        //     keysPath = optarg;
        //     break;
        // }

        case 'N': // Number of shards.
        { 
            char *strtolPtr;
            nShards = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') ||
                (nShards <= 0)) {
                fprintf(stderr, "option -N requires a numeric arg\n");
            }
            break;
        }

        

        case 'n': // Number of replicas of each shard.
        {
            char *strtolPtr;
            nReplicas = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') ||
                (nShards <= 0)) {
                fprintf(stderr, "option -n requires a numeric arg\n");
            }
            break;
        }

        case 'd': // Duration in seconds to run.
        { 
            char *strtolPtr;
            duration = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') ||
                (duration <= 0)) {
                fprintf(stderr, "option -d requires a numeric arg\n");
            }
            break;
        }

        // case 'k': // Number of keys to operate on.
        // {
        //     char *strtolPtr;
        //     nKeys = strtoul(optarg, &strtolPtr, 10);
        //     if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        //         (nKeys <= 0)) {
        //         fprintf(stderr, "option -k requires a numeric arg\n");
        //     }
        //     break;
        // }

        case 's': // Simulated clock skew.
        {
            char *strtolPtr;
            skew = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (skew < 0))
            {
                fprintf(stderr,
                        "option -s requires a numeric arg\n");
            }
            break;
        }

        case 'e': // Simulated clock error.
        {
            char *strtolPtr;
            error = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (error < 0))
            {
                fprintf(stderr,
                        "option -e requires a numeric arg\n");
            }
            break;
        }

        // case 'z': // Zipf coefficient for key selection.
        // {
        //     char *strtolPtr;
        //     alpha = strtod(optarg, &strtolPtr);
        //     if ((*optarg == '\0') || (*strtolPtr != '\0'))
        //     {
        //         fprintf(stderr,
        //                 "option -z requires a numeric arg\n");
        //     }
        //     break;
        // }

        case 'D': // ratio of distributed transaction
        {
            char *strtolPtr;
            dis_ratio = strtod(optarg, &strtolPtr);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr,
                        "option -D requires a numeric arg\n");
            }
            break;
        }

        case 'r': // Preferred closest replica.
        {
            char *strtolPtr;
            closestReplica = strtod(optarg, &strtolPtr);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr,
                        "option -r requires a numeric arg\n");
            }
            break;
        }

        case 'm': // Mode to run in [occ/lock/...]
        {
            if (strcasecmp(optarg, "txn-l") == 0) {
                mode = MODE_TAPIR;
            } else if (strcasecmp(optarg, "txn-s") == 0) {
                mode = MODE_TAPIR;
            } else if (strcasecmp(optarg, "qw") == 0) {
                mode = MODE_WEAK;
            } else if (strcasecmp(optarg, "occ") == 0) {
                mode = MODE_STRONG;
                strongmode = strongstore::MODE_OCC;
            } else if (strcasecmp(optarg, "lock") == 0) {
                mode = MODE_STRONG;
                strongmode = strongstore::MODE_LOCK;
            } else if (strcasecmp(optarg, "span-occ") == 0) {
                mode = MODE_STRONG;
                strongmode = strongstore::MODE_SPAN_OCC;
            } else if (strcasecmp(optarg, "span-lock") == 0) {
                mode = MODE_STRONG;
                strongmode = strongstore::MODE_SPAN_LOCK;
            } else {
                fprintf(stderr, "unknown mode '%s'\n", optarg);
                exit(0);
            }
            break;
        }

        case 't':
        {
            if (strcasecmp(optarg, "fast") == 0) {
                tpcMode = TpcMode::MODE_FAST;
            } else if (strcasecmp(optarg, "slow") == 0) {
                tpcMode = TpcMode::MODE_SLOW;
            } else if (strcasecmp(optarg, "parallel") == 0) {
                tpcMode = TpcMode::MODE_PARALLEL;
            } else if (strcasecmp(optarg, "carousel") == 0) {
                tpcMode = TpcMode::MODE_CAROUSEL;
            } else if (strcasecmp(optarg, "rc") == 0) {
                tpcMode = TpcMode::MODE_RC;
            } else {
                fprintf(stderr, "unknown tpcMode '%s'\n", optarg);
            }
            break;
        }

        default:
            fprintf(stderr, "Unknown argument %s\n", argv[optind]);
            break;
        }
    }

    // if (mode == MODE_TAPIR) {
    //     client = new tapirstore::Client(configPath, nShards,
    //                 closestReplica, TrueTime(skew, error));
    // } else if (mode == MODE_WEAK) {
    //     client = new weakstore::Client(configPath, nShards,
    //                 closestReplica);
    // } else 
    if (mode == MODE_STRONG) {
        client = new strongstore::Client(strongmode, tpcMode, configPath,
                    nShards, closestReplica, nReplicas, TrueTime(skew, error), true);
    } else {
        fprintf(stderr, "option -m is required\n");
        exit(0);
    }

    // generate data

    struct timeval t0, t1, t2;
    int nTransactions = 0; // Number of transactions attempted.
    int ttype; // Transaction type.
    int ret;
    bool status;
    int distributed;

    gettimeofday(&t0, NULL);
    srand(t0.tv_sec + t0.tv_usec);

    while(1) {
        // Begin a transaction.
        client->Begin();
        gettimeofday(&t1, NULL);
        status = true;

        // Decide which type of retwis transaction it is going to be.
        ttype = rand() % 100;
        distributed = rand() % 100;

        // new 48, pay 95
        if(ttype <= 48) {
            NewOrder newOrder(client);
  
            if((double)distributed <= dis_ratio * 100){
                newOrder.GenInputData(false);
            } else {
                newOrder.GenInputData(true);
            }
            ret = newOrder.RunTxn(&neworders);

            if(ret){
                status = false;
            }
            ttype = 1;
        } else if (ttype <= 95) {
            Payment payment(client);
            if((double)distributed <= dis_ratio * 100){
                payment.GenInputData(false);
            } else {
                payment.GenInputData(true);
            }
            ret = payment.RunTxn();

            if(ret){
                Warning("Payment Abort");
                status = false;
            }
            ttype = 2;
        } else {
            Delivery delivery(client);
            if((double)distributed <= dis_ratio * 100){
                delivery.GenInputData(false);
            } else {
                delivery.GenInputData(true);
            }
            ret = delivery.RunTxn(&neworders);

            if(ret){
                Warning("Delivery Abort");
                status = false;
            }
            ttype = 3;
        }

        if(status){
            Debug("Start commit");
            status = client->Commit();
        } else {
            Debug("Aborting transaction due to failed read");   
        }
        gettimeofday(&t2, NULL);

        long latency = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);

        int retries = 0;
        if (!client->Stats().empty()) {
            retries = client->Stats()[0];
        }
        Debug("retries: %d", retries);

        fprintf(stderr, "%d %ld.%06ld %ld.%06ld %ld %d %d %d", ++nTransactions, t1.tv_sec,
                t1.tv_usec, t2.tv_sec, t2.tv_usec, latency, status?1:0, ttype, retries);
        fprintf(stderr, "\n");

        if (((t2.tv_sec-t0.tv_sec)*1000000 + (t2.tv_usec-t0.tv_usec)) > duration*1000000) 
            break;
    }

    fprintf(stderr, "# Client exiting..\n");
    return 0;

}