

#include "store/weakstore/store.h"

namespace weakstore {

using namespace std;

Store::Store() { }
Store::~Store() { }

int
Store::Get(uint64_t id, const string &key, string &value)
{
    Debug("[%lu] GET %s", id, key.c_str());
    string val;
    if (store.get(key, val)) {
        value = val;
        return REPLY_OK;
    }
    return REPLY_FAIL;
}


int
Store::Put(uint64_t id, const string &key, const string &value)
{
    Debug("[%lu] PUT %s %s", id, key.c_str(), value.c_str());
    store.put(key, value);
    return REPLY_OK;
}

void
Store::Load(const string &key, const string &value)
{
    store.put(key, value);
}

} // namespace weakstore
