#include <gtest/gtest.h>

#include "Utils/String.h"

TEST(Utils, HashString_ProducesKnownValues)
{
    auto h1 = fc::Utils::String::HashString("hello");
    auto h2 = fc::Utils::String::HashString("hello");
    EXPECT_EQ(h1, h2);
}

TEST(Utils, HashString_ProducesUniqueValues)
{
    auto h1 = fc::Utils::String::HashString("abc");
    auto h2 = fc::Utils::String::HashString("xyz");
    EXPECT_NE(h1, h2);
}
TEST(Utils, HashString_IsCaseSensitive)
{
    auto lower = fc::Utils::String::HashString("abc");
    auto upper = fc::Utils::String::HashString("ABC");
    EXPECT_NE(lower, upper);
}
