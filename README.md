# D2PC

This repository includes code implementing D2PC, Carousel, OCC-Store(OCC+2PC), and 2PL-Store(2PC+2PC). Both approaches support geo-distributed transaction processing.

The code is based on TAPIR, the SOSP 2015 paper, ["Building Consistent Transactions with
Inconsistent Replication."](http://dl.acm.org/authorize?N93281).

The repo is structured as follows:

- /lib - the transport library for communication between nodes. This
  includes UDP based network communcation as well as the ability to
  simulate network conditions on a local machine, including packet
  delays and reorderings.

- /replication - replication library for the distributed stores
  - /vr - implementation of viewstamped replication protocol
  - /ir - implementation of inconsistent replication protocol
  - /lr - implementation of leader based replication protocol
  - /commit - implementation of co-coordinators

- /store - partitioned/sharded distributed store
  - /common - common data structures, backing stores and interfaces for all of stores
  - /strongstore - implementation of both an OCC-based and locking-based 2PC transactional
  storage system, designed to work with LR

## Compiling & Running
You can compile all of the D2PC executables by running make in the root directory

D2PC depends on protobufs, libevent and openssl, so you will need the following development libraries:
- libprotobuf-dev
- libevent-openssl
- libevent-pthreads
- libevent-dev
- libssl-dev
- protobuf-compiler
