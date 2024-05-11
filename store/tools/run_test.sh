#!/bin/bash

trap '{
  echo "\nKilling all clients.. Please wait..";
  for host in ${clients[@]}
  do
    ssh $host "killall -9 $client";
    ssh $host "killall -9 $client";
  done

  echo "\nKilling all replics.. Please wait..";
  for host in ${servers[@]}
  do
    ssh $host "killall -9 server";
  done
}' INT

# Paths to source code and logfiles.
srcdir="/root/D2PC"
logdir="/root/log"

# Machines on which replicas are running.
replicas=("47.99.136.66" "47.89.249.0" "8.209.111.83")

# Machines on which clients are running.
clients=("121.199.75.14" "47.251.49.252" "8.211.5.151")
#clients=("121.199.75.14")

#client="retwisClient"
client="benchClient"    # Which client (benchClient, retwisClient, etc)
store="strongstore"      # Which store (strongstore, weakstore, tapirstore)
mode="occ"            # Mode for storage system.
tpcmode="parallel"      # Mode for commit algorithm (fast/slow/parellel)
workloaddata=0
keypath="/root/D2PC/store/tools/keys"
#keypath="/root/D2PC/store/tools/tpcc_data"

nshard=3     # number of shards
nreplica=3   # number of replicas
nclient=50   # number of clients to run (per machine)
nkeys=5000000 # number of keys to use
rtime=20     # duration to run
disratio=0.3

tlen=10       # transaction length
wper=100       # writes percentage
err=0        # error
skew=0       # skew
zalpha=0.6    # zipf alpha (-1 to disable zipf and enable uniform)

# Print out configuration being used.
echo "Configuration:"
echo "Shards: $nshard"
echo "Clients per host: $nclient"
echo "Threads per client: $nthread"
echo "Keys: $nkeys"
echo "Transaction Length: $tlen"
echo "Write Percentage: $wper"
echo "Error: $err"
echo "Skew: $skew"
echo "Zipf alpha: $zalpha"
echo "Skew: $skew"
echo "Client: $client"
echo "Store: $store"
echo "Mode: $mode"


# Generate keys to be used in the experiment.
# echo "Generating random keys.."
# python key_generator.py $nkeys > keys


# Start all replicas and timestamp servers
echo "Starting TimeStampServer replicas.."
$srcdir/store/tools/start_replica.sh tss $srcdir/store/tools/shard.tss.config \
  "$srcdir/timeserver/timeserver" $logdir

for ((i=0; i<$nshard; i++))
do
  echo "Starting shard$i replicas.."
  $srcdir/store/tools/start_replica.sh shard$i $srcdir/store/tools/shard$i.config \
    "$srcdir/store/$store/server -m $mode -t $tpcmode -S $nshard -n $i -N $nreplica\
    -f $keypath -k $nkeys -e $err -s $skew -w $workloaddata" $logdir
done

echo "Starting coordinator replicas.."
$srcdir/store/tools/start_replica.sh coordinator $srcdir/store/tools/shard.coor.config \
  "$srcdir/replication/commit/coorserver -i $i -N $nshard" $logdir

# Wait a bit for all replicas to start up
sleep 5


# Run the clients
echo "Running the client(s)"
count=0
replica=0
for host in ${clients[@]}
do
  ssh $host "$srcdir/store/tools/start_client.sh \"$srcdir/store/benchmark/$client \
  -c $srcdir/store/tools/shard -N $nshard -f $srcdir/store/tools/keys \
  -d $rtime -r $replica -t $tpcmode -n $nreplica -l $tlen -w $wper -k $nkeys -m $mode -e $err -s $skew -z $zalpha -D $disratio\" \
  $count $nclient $logdir"

  let replica=$replica+1
  let count=$count+$nclient
done


# Wait for all clients to exit
echo "Waiting for client(s) to exit"
for host in ${clients[@]}
do
  ssh $host "$srcdir/store/tools/wait_client.sh $client"
done


# Kill all replicas
echo "Cleaning up"
$srcdir/store/tools/stop_replica.sh $srcdir/store/tools/shard.tss.config > /dev/null 2>&1
for ((i=0; i<$nshard; i++))
do
  $srcdir/store/tools/stop_replica.sh $srcdir/store/tools/shard$i.config > /dev/null 2>&1
done
$srcdir/store/tools/stop_replica.sh $srcdir/store/tools/shard.coor.config > /dev/null 2>&1


# Process logs
echo "Processing logs"
cat $logdir/client.*.log | sort -g -k 3 > $logdir/client.log
rm -f $logdir/client.*.log

python $srcdir/store/tools/process_logs.py $logdir/client.log $rtime

