

#include "store/common/backend/lockserver.h"

#include <gtest/gtest.h>

TEST(LockServer, ReadLock)
{
    LockServer s;

    EXPECT_TRUE(s.lockForRead("x", 1));
    EXPECT_TRUE(s.lockForRead("x", 2));   
    EXPECT_FALSE(s.lockForWrite("x", 3));
}

TEST(LockServer, WriteLock)
{
    LockServer s;
    
    EXPECT_TRUE(s.lockForWrite("x", 1));
    EXPECT_FALSE(s.lockForRead("x", 2));   
    EXPECT_FALSE(s.lockForWrite("x", 3));    
}
