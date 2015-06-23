#include <iostream>
#include <gtest/gtest.h>
#include "queue1c.h"

using namespace std;

TEST(SquareRootTest, PositiveNos)
{
    queue1c<int> q(10);
    q.push(1);
    q.push(2);

    std::vector<int> res;
    q.get_all(res);

    ASSERT_EQ( 2u, res.size() );
    EXPECT_EQ( 1, res[0]);
    EXPECT_EQ( 2, res[1]);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}