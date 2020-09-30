#include <gtest/gtest.h>

#undef NDEBUG
#define protected public
#define private   public
#include <heco_n_map_stable.h>
#undef protected
#undef private

using namespace heco;

TEST(HeterogeneousContainer_n, insert_and_get)
{
    HeterogeneousContainer_n c;
    c.insert(1.5, 2.6, 24.2);
    c.insert(std::vector<int>{1, 2, 3, 5});
    c.vector<double>().push_back(3.14);
    EXPECT_EQ(c.vector<double>(3), 3.14);
    EXPECT_EQ(c.vector<int>().back(), 5);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
