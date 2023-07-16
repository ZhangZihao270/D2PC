#include "replication/commit/coordinator.h"
#include <algorithm>

using namespace std;

int
main(int argc, char **argv)
{
    int index = -1;
    unsigned int maxShard=1;
    const char *configPath = NULL;
    std::string coordinatorConfigPath;

    int opt;
    while ((opt = getopt(argc, argv, "c:i:N:")) != -1) {
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

        case 'N':
        {
            char *strtolPtr;
            maxShard = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (maxShard <= 0))
            {
                fprintf(stderr, "option -e requires a numeric arg\n");
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

    
    fprintf(stderr, "coordinator config path: %s\n", configPath);
    std::ifstream coordinatorConfigStream(configPath);
    if (coordinatorConfigStream.fail()) {
        fprintf(stderr, "unable to read coordinator configuration file: %s\n", coordinatorConfigPath.c_str());
    }
    transport::Configuration coordinatorConfig(coordinatorConfigStream);

    UDPTransport coordinator_transport(0.0, 0.0, 0);

    std::vector<transport::Configuration> config;
    std::vector<Transport *> shardTransport;

    std::string str(configPath);
    std::string config_prefix = str.substr(0, str.length()-12);
    for(int i = 0; i < maxShard; i++){
        std::string cpath = config_prefix + std::to_string(i) + ".config";
        fprintf(stderr, "shard %d config path: %s\n", i, cpath.c_str());

        std::ifstream cStream(cpath);
        if (cStream.fail()) {
            fprintf(stderr, "unable to read shard %d configuration file: %s\n", i, cpath.c_str());
        }
        config.push_back(transport::Configuration(cStream));
        shardTransport.push_back(new UDPTransport(0.0, 0.0, 0));
    }


    replication::commit::CoordinatorReplica coordinator(config, coordinatorConfig, index,
        shardTransport, &coordinator_transport);

    coordinator_transport.Run();

    return 0;
}
