

#include "store/common/backend/txnstore.h"

using namespace std;

TxnStore::TxnStore() {}
TxnStore::~TxnStore() {}

int
TxnStore::Get(uint64_t id, const string &key, pair<Timestamp, string> &value)
{
    Panic("Unimplemented GET");
    return 0;
}

int
TxnStore::Get(uint64_t id, const string &key, const Timestamp &timestamp,
    pair<Timestamp, string> &value)
{
    Panic("Unimplemented GET");
    return 0;
}


int
TxnStore::Get(uint64_t id, const string &key, pair<Timestamp, string> &value, uint64_t &dependency)
{
    Panic("Unimplemented GET");
    return 0;
}

int
TxnStore::Get(uint64_t id, const string &key, const Timestamp &timestamp,
    pair<Timestamp, string> &value, uint64_t &dependency)
{
    Panic("Unimplemented GET");
    return 0;
}

int
TxnStore::Put(uint64_t id, const string &key, const string &value)
{
    Panic("Unimplemented PUT");
    return 0;
}

int
TxnStore::Prepare(uint64_t id, const Transaction &txn)
{
    Panic("Unimplemented PREPARE");
    return 0;
}

int
TxnStore::Prepare(uint64_t id, const Transaction &txn, bool isLeader)
{
    Panic("Unimplemented PREPARE");
    return 0;
}

int
TxnStore::Prepare(uint64_t id, const Transaction &txn,
    const Timestamp &timestamp, Timestamp &proposed)
{
    Panic("Unimplemented PREPARE");
    return 0;
}

void
TxnStore::Commit(uint64_t id, uint64_t timestamp)
{
    Panic("Unimplemented COMMIT");
}

void
TxnStore::PreCommit(uint64_t id)
{
    Panic("Unimplemented PRE-COMMIT");
}

void
TxnStore::Abort(uint64_t id, const Transaction &txn)
{
    Panic("Unimplemented ABORT");
}

void
TxnStore::Load(const string &key, const string &value, const Timestamp &timestamp)
{
    Panic("Unimplemented LOAD");
}
