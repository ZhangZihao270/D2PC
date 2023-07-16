

#include "store/common/backend/versionstore.h"

#include <gtest/gtest.h>

TEST(VersionedKVStore, Get)
{
    VersionedKVStore store;
    std::pair<Timestamp, std::string> val;

    store.put("test1", "abc", Timestamp(10));
    EXPECT_TRUE(store.get("test1", val));
    EXPECT_EQ(val.second, "abc");
    EXPECT_EQ(Timestamp(10), val.first); 

    store.put("test2", "def", Timestamp(10));
    EXPECT_TRUE(store.get("test2", val));
    EXPECT_EQ(val.second, "def");
    EXPECT_EQ(Timestamp(10), val.first); 

    store.put("test1", "xyz", Timestamp(11));
    EXPECT_TRUE(store.get("test1", val));
    EXPECT_EQ(val.second, "xyz");
    EXPECT_EQ(Timestamp(11), val.first); 
    
    EXPECT_TRUE(store.get("test1", Timestamp(10), val));
    EXPECT_EQ(val.second, "abc");
}
