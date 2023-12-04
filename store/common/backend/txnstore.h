

#ifndef _TXN_STORE_H_
#define _TXN_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"

class TxnStore
{
public:

    TxnStore();
    virtual ~TxnStore();

    // add key to read set
    virtual int Get(uint64_t id, const std::string &key,
        std::pair<Timestamp, std::string> &value);

    virtual int Get(uint64_t id, const std::string &key,
        const Timestamp &timestamp, std::pair<Timestamp, std::string> &value);

    // For parallel mode
    // add key to read set
    virtual int Get(uint64_t id, const std::string &key,
        std::pair<Timestamp, std::string> &value, uint64_t &dependency);

    virtual int Get(uint64_t id, const std::string &key,
        const Timestamp &timestamp, std::pair<Timestamp, std::string> &value, uint64_t &dependency);

    // add key to write set
    virtual int Put(uint64_t id, const std::string &key,
        const std::string &value);

    // check whether we can commit this transaction (and lock the read/write set)
    virtual int Prepare(uint64_t id, const Transaction &txn);

    virtual int Prepare(uint64_t id, const Transaction &txn, bool isLeader);

    virtual int Prepare(uint64_t id, const Transaction &txn,
        const Timestamp &timestamp, Timestamp &proposed);

    // commit the transaction
    virtual std::vector<uint64_t> Commit(uint64_t id, uint64_t timestamp = 0);

    // precommit the transaction to end the transaction critical path early
    virtual void PreCommit(uint64_t id);

    // abort a running transaction
    virtual std::vector<uint64_t> Abort(uint64_t id, const Transaction &txn = Transaction());

    // load keys
    virtual void Load(const std::string &key, const std::string &value,
        const Timestamp &timestamp);
};

#endif /* _TXN_STORE_H_ */
