#include <gtest/gtest.h>

#undef NDEBUG
#define protected public
#define private   public
#include <heco_1_map_stable.h>
#undef protected
#undef private

using namespace heco;

struct A
{
    int x;
    char c;
    ~A() {};
};
static_assert(std::is_trivially_destructible_v < A > == false);
static_assert(alignof(A) == 4);
static_assert(sizeof(A) == 8);

struct B
{
    double x;
    int y[4];
    A z;
};
static_assert(std::is_trivially_destructible_v < B > == false);

struct alignas(8) C { int v; };
static_assert(alignof(C) == 8);
static_assert(sizeof(C) == 8);

TEST(HeterogeneousContainer, insert)
{
    double X00 = 5.63454f;
    int X01 = 218762532;

    HeterogeneousContainer container;
    EXPECT_EQ(container.insert(X00), X00);
    EXPECT_EQ(container.insert(X01), X01);
    EXPECT_EQ(container.insert<float>(), 0);
    EXPECT_EQ(container.insert<char>(), 0);
}

TEST(HeterogeneousContainer, insert_all)
{

    HeterogeneousContainer container;
    auto&& [c, d, b] = container.insert(char{ 'a' }, double{ 1 }, bool{ true });
    EXPECT_EQ(c, 'a');
    EXPECT_EQ(d, 1);
    auto& i = container.insert(int{ 5 });
    EXPECT_EQ(i, 5);
    struct empty_1 {};
    struct empty_2 {};
    container.insert(empty_1{}, empty_2{});
    EXPECT_EQ(container.contains<empty_1>(), true);
    EXPECT_EQ(container.contains<empty_2>(), true);
}

TEST(HeterogeneousContainer, insert_or_assign)
{
    HeterogeneousContainer container;
    {
        auto&& c = container.insert(char{ 'a' });
        EXPECT_EQ(c, 'a');
    }
    {
        auto&& c = container.insert_or_assign<char>('b');
        EXPECT_EQ(c, 'b');
    }
    {
        auto&& d = container.insert_or_assign<double>(3.14);
        EXPECT_EQ(d, 3.14);
    }
    {
        auto&& d = container.insert_or_assign<double>(42.);
        EXPECT_EQ(d, 42.);
        EXPECT_EQ(container.get<double>(), 42.);
    }
}

TEST(HeterogeneousContainer, get)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousContainer container;
    container.insert(X00);
    container.insert(X01);
    auto&& v = container.get<double>();
    EXPECT_EQ(v, X00);
    EXPECT_EQ(container.get<int>(), X01);
    auto&& [vd, vi] = container.get<double, int>();
    static_assert(std::is_same_v<decltype(vd), double&>);
    static_assert(std::is_same_v<decltype(vi), int&>);
    EXPECT_EQ(vd, X00);
    EXPECT_EQ(vi, X01);
    auto&& vdd = container.get<double>();
    static_assert(std::is_same_v<decltype(vdd), double&>);
    EXPECT_EQ(vdd, X00);
}

TEST(HeterogeneousContainer, const_get)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousContainer hc;
    hc.insert(X00);
    hc.insert(X01);
    {
        const HeterogeneousContainer& container = hc;
        EXPECT_EQ(container.get<const double>(), X00);
        EXPECT_EQ(container.get<const int>(), X01);
        auto&& [vd, vi] = container.get<double, const int>();
        static_assert(std::is_same_v<decltype(vd), double&>);
        static_assert(std::is_same_v<decltype(vi), const int&>);
        EXPECT_EQ(vd, X00);
        EXPECT_EQ(vi, X01);
        {
            auto&& vdd = container.get<const double>();
            static_assert(std::is_same_v<decltype(vdd), const double&>);
            EXPECT_EQ(vdd, X00);
        }
        {
            auto&& vdd = static_cast<const HeterogeneousContainer&>(container).get<const double>();
            static_assert(std::is_same_v<decltype(vdd), const double&>);
            EXPECT_EQ(vdd, X00);
        }
    }
}

TEST(HeterogeneousContainer, has)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousContainer container;
    container.insert(X00);
    container.insert(X01);
    if (auto ptr = container.has<double>())
        *ptr += 5.;
    EXPECT_EQ(container.get<double>(), X00 + 5.);
    auto ptr = container.has<bool>();
    EXPECT_EQ(ptr, nullptr);
}

TEST(HeterogeneousContainer, contains)
{
    double X00 = 5.63454f;

    HeterogeneousContainer container;
    container.insert(X00);
    container.insert(A{});
    EXPECT_EQ(container.contains<double>(), true);
    EXPECT_EQ(container.contains<int>(), false);
    EXPECT_EQ(container.contains<A>(), true);
    EXPECT_EQ(container.contains<B>(), false);
    bool b = false;
    b = container.contains<A, double>();
    EXPECT_EQ(b, true);
    b = container.contains<A, B>();
    EXPECT_EQ(b, false);
    b = container.contains<int, B>();
    EXPECT_EQ(b, false);
}

TEST(HeterogeneousContainer, modify_value_simple)
{
    double magic_00 = 5.1;
    int magic_01 = 5;

    const double X00 = 0;
    int X01 = 218762532;

    HeterogeneousContainer container;
    {
        auto& x = container.insert(X00);
        x += magic_00;
    }
    EXPECT_EQ(container.get<decltype(X00)>(), magic_00);
    {
        auto& x = container.insert(X01);
        x += magic_01;
    }
    EXPECT_EQ(container.get<decltype(X01)>(), magic_01 + X01);
}

TEST(HeterogeneousContainer, modify_value_complex)
{
    double magic_00 = 5.1;
    int magic_01 = 5;

    A a{ -1 };
    B b{ 1.5, { 0, 1, 2, 3 }, a };

    HeterogeneousContainer container;
    {
        auto& x = container.insert(a);
        x.x += magic_01;
    }
    EXPECT_EQ(container.get<decltype(a)>().x, a.x + magic_01);
    {
        auto& x = container.insert(b);
        x.x += magic_00;
        x.y[3] = magic_01;
        x.z.x += magic_01;
    }
    EXPECT_EQ(container.get<decltype(b)>().x, b.x + magic_00);
    EXPECT_EQ(container.get<decltype(b)>().y[3], magic_01);
    EXPECT_EQ(container.get<decltype(b)>().z.x, a.x + magic_01);
}

TEST(HeterogeneousContainer, const_access)
{
    double X00 = 5.63454f;
    HeterogeneousContainer container;
    container.insert(X00);
    auto&& v1 = container.get<const double>();
    EXPECT_EQ(v1, X00);
    static_assert(std::is_same_v<decltype(v1), const double&>);

    const auto& const_container = container;
    auto&& v2 = const_container.get<double>();
    v2 += 0.5;              
    EXPECT_EQ(v2, X00+0.5);
    static_assert(std::is_same_v<decltype(v2), double&>);
}

TEST(HeterogeneousContainer, move_container)
{
    HeterogeneousContainer a;
    using vec = std::vector<int>;
    a.insert(vec{5,25});
    a.insert(42);
    //↓ is insert valid
    EXPECT_EQ(a.get<vec>()[0], 5);
    EXPECT_EQ(a.get<vec>()[1], 25);
    EXPECT_EQ(a.get<int>(), 42);
    HeterogeneousContainer b{std::move(a)};
    //↓ is move valid
    EXPECT_EQ(b.get<vec>()[0], 5);
    EXPECT_EQ(b.get<vec>()[1], 25);
    EXPECT_EQ(b.get<int>(), 42);
    //↓ is original container metadata reset
    EXPECT_EQ(a.has<vec>(), nullptr);
    EXPECT_EQ(a.has<int>(), nullptr);
    //↓ is original container data reset
    EXPECT_EQ(a.data.size(), 0);
    //↓ are containers really dissociated
    a.insert_or_assign(56);
    EXPECT_EQ(b.get<int>(), 42);
}

struct test_mv_dtor {
    test_mv_dtor() = default;
    test_mv_dtor(const test_mv_dtor&) = delete;
    test_mv_dtor& operator=(test_mv_dtor&&) { moved = true; };
    test_mv_dtor(test_mv_dtor&&) { moved = true; };
    ~test_mv_dtor() { destroyed = true; }
    static inline bool moved = false;
    static inline bool destroyed = false;
};

TEST(HeterogeneousContainer, destruction)
{
    HeterogeneousContainer container0;
    container0.insert<test_mv_dtor>();
    auto* container = new HeterogeneousContainer(std::move(container0));
    EXPECT_EQ(test_mv_dtor::moved, false);
    delete container;
    EXPECT_EQ(test_mv_dtor::destroyed, true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
