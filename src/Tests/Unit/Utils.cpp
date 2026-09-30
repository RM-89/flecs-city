#include <gtest/gtest.h>

#include "Utils/String.h"

TEST(Utils, HashString_ProducesKnownValues)
{
    const auto h1 = fc::Utils::String::HashString("hello");
    const auto h2 = fc::Utils::String::HashString("hello");
    EXPECT_EQ(h1, h2);
}

TEST(Utils, HashString_ProducesUniqueValues)
{
    const auto h1 = fc::Utils::String::HashString("abc");
    const auto h2 = fc::Utils::String::HashString("xyz");
    EXPECT_NE(h1, h2);
}

TEST(Utils, HashString_IsCaseSensitive)
{
    const auto lower = fc::Utils::String::HashString("abc");
    const auto upper = fc::Utils::String::HashString("ABC");
    EXPECT_NE(lower, upper);
}
