

#include "store/common/backend/kvstore.h"

#include <gtest/gtest.h>

TEST(KVStore, Put)
{
    KVStore store;

    EXPECT_TRUE(store.put("test1", "abc"));
    EXPECT_TRUE(store.put("test2", "def"));
    EXPECT_TRUE(store.put("test1", "xyz"));
    EXPECT_TRUE(store.put("test3", "abc"));
}

TEST(KVStore, Get)
{
    KVStore store;
    std::string val;

    EXPECT_TRUE(store.put("test1", "abc"));
    EXPECT_TRUE(store.get("test1", val));
    EXPECT_EQ(val, "abc");

    EXPECT_TRUE(store.put("test2", "def"));
    EXPECT_TRUE(store.get("test2", val));
    EXPECT_EQ(val, "def");

    EXPECT_TRUE(store.put("test1", "xyz"));
    EXPECT_TRUE(store.get("test1", val));
    EXPECT_EQ(val, "xyz");
}

