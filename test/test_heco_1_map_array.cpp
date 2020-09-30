#include <gtest/gtest.h>

#undef NDEBUG
#define protected public
#define private   public
#include <heco_1_map_array.h>
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

TEST(HeterogeneousArray, reserve)
{
    HeterogeneousArray container;
    container.reserve(8, 0);
    EXPECT_EQ(container.data.capacity(), 8);

    container.reserve(16, 8, 1);
    EXPECT_EQ(container.data.capacity(), 16);

    container.reserve(8, 2, 1);
    //container.reserve<short,const short&>();
    container.reserve<double, char, int>();
    container.reserve<A, B>();
}

TEST(HeterogeneousArray, insert)
{
    double X00 = 5.63454f;
    int X01 = 218762532;

    HeterogeneousArray container;
    EXPECT_EQ(container.insert(X00), X00);
    EXPECT_EQ(container.insert(X01), X01);
    EXPECT_EQ(container.insert<float>(), 0);
    EXPECT_EQ(container.insert<char>(), 0);
}

TEST(HeterogeneousArray, insert_all)
{

    HeterogeneousArray container;
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

TEST(HeterogeneousArray, insert_or_assign)
{
    HeterogeneousArray container;
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

TEST(HeterogeneousArray, get)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousArray container;
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

TEST(HeterogeneousArray, const_get)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousArray hc;
    hc.insert(X00);
    hc.insert(X01);
    {
        const HeterogeneousArray& container = hc;
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
            auto&& vdd = static_cast<const HeterogeneousArray&>(container).get<const double>();
            static_assert(std::is_same_v<decltype(vdd), const double&>);
            EXPECT_EQ(vdd, X00);
        }
    }
}

TEST(HeterogeneousArray, offset_of)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousArray container;
    container.insert(X00);
    container.insert(X01);
    auto offset_int = container.offset_of<int>();
    EXPECT_EQ(offset_int, sizeof(X00));
    auto offsets = container.offset_of<int, double>();
    EXPECT_EQ(offsets[0], sizeof(X00));
    EXPECT_EQ(offsets[1], 0);
}

TEST(HeterogeneousArray, get_from_offset)
{
    double X00 = 5.63454;
    int X01 = 218762532;

    HeterogeneousArray container;
    container.insert(X00);
    container.insert(X01);
    auto offset_int = container.offset_of<int>();
    auto offset_dbl = container.offset_of<double>();
    EXPECT_EQ(container.get<int>(offset_int), X01);
    EXPECT_EQ(container.get<double>(offset_dbl), X00);
    auto&& c = container.get<int>(offset_int);
    EXPECT_EQ(c, X01);
    auto&& [vi, vd] = container.get<int, double>(offset_int, offset_dbl);
    static_assert(std::is_same_v<decltype(vd), double&>);
    static_assert(std::is_same_v<decltype(vi), int&>);
    EXPECT_EQ(vd, X00);
    EXPECT_EQ(vi, X01);
}

TEST(HeterogeneousArray, contains)
{
    double X00 = 5.63454f;

    HeterogeneousArray container;
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

TEST(HeterogeneousArray, modify_value_simple)
{
    double magic_00 = 5.1;
    int magic_01 = 5;

    const double X00 = 0;
    int X01 = 218762532;

    HeterogeneousArray container;
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

TEST(HeterogeneousArray, modify_value_complex)
{
    double magic_00 = 5.1;
    int magic_01 = 5;

    A a{ -1 };
    B b{ 1.5, { 0, 1, 2, 3 }, a };

    HeterogeneousArray container;
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

TEST(HeterogeneousArray, const_access)
{
    double X00 = 5.63454f;
    HeterogeneousArray container;
    container.insert(X00);
    auto&& v = container.get<const double>();
    EXPECT_EQ(v, X00);
    static_assert(std::is_same_v<decltype(v), const double&>);
}

TEST(HeterogeneousArray, non_trivially_destructible)
{
    HeterogeneousArray container;
    EXPECT_EQ(container.destructors.size(), 0);
    container.insert(A{});
    EXPECT_EQ(container.destructors.size(), 1);
    container.insert(1.f);
    EXPECT_EQ(container.destructors.size(), 2);
    container.insert<B>();
    EXPECT_EQ(container.destructors.size(), 3);

    container.destruct<A>();
    EXPECT_EQ(container.destructors.size(), 2);
    container.destruct<B>();
    EXPECT_EQ(container.destructors.size(), 1);
    container.destruct<float>();
    EXPECT_EQ(container.destructors.size(), 0);

    container.assign(A{});
    container.assign(1.f);
    container.assign<B>();

    container.destruct<B>();
    container.destruct<float>();
    container.destruct<A>();

    container.insert_or_assign<A>();
    container.insert_or_assign<float>(1.6f);
    container.insert_or_assign<B>();
}

TEST(HeterogeneousArray, aligned_allocation)
{
    {// do_allocate_1
        HeterogeneousArray container;
        container.insert(A{ 5 });
        size_t offset = container.reserve<C>();
        std::uintptr_t ptr_sta = std::uintptr_t(container.data.data());
        std::uintptr_t ptr_end = std::uintptr_t(container.data.data() + offset);
        EXPECT_EQ(ptr_end % alignof(C), 0);
        EXPECT_EQ(ptr_end + sizeof(C) - ptr_sta, container.data.size());
    }
    {// do_allocate_n
        HeterogeneousArray container;
        container.insert<char>('a');
        auto offsets = container.reserve<A, C>();
        {
            std::uintptr_t ptr_end = std::uintptr_t(container.data.data() + offsets[0]);
            EXPECT_EQ(ptr_end % alignof(A), 0);
        }
        {
            std::uintptr_t ptr_end = std::uintptr_t(container.data.data() + offsets[1]);
            EXPECT_EQ(ptr_end % alignof(C), 0);
        }
        EXPECT_TRUE(container.data.size() >= sizeof(A) + sizeof(C));
    }
}

TEST(HeterogeneousArray, empty_type)
{
    struct D {};
    static_assert(std::is_empty_v < D > == true);
    static_assert(alignof(D) == 1);
    static_assert(sizeof(D) == 1);

    HeterogeneousArray container;
    container.insert(D{});
    EXPECT_EQ(container.offsets.size(), 1);
    container.insert(D{});
    EXPECT_EQ(container.offsets.size(), 1);
    container.insert(A{});
    EXPECT_EQ(container.offsets.size(), 2);
    ASSERT_DEATH(container.insert(A{}), "");
    EXPECT_EQ(container.contains<D>(), true);
    EXPECT_EQ(container.contains<A>(), true);
    EXPECT_EQ(container.data.size(), sizeof(A));
    container.destruct<D>();
    EXPECT_EQ(container.contains<D>(), false);
}

TEST(HeterogeneousArray, move_container)
{
    HeterogeneousArray a;
    using vec = std::vector<int>;
    a.insert(vec{5,25});
    a.insert(42);
    //↓ is insert valid
    EXPECT_EQ(a.get<vec>()[0], 5);
    EXPECT_EQ(a.get<vec>()[1], 25);
    EXPECT_EQ(a.get<int>(), 42);
    HeterogeneousArray b{std::move(a)};
    //↓ is move valid
    EXPECT_EQ(b.get<vec>()[0], 5);
    EXPECT_EQ(b.get<vec>()[1], 25);
    EXPECT_EQ(b.get<int>(), 42);
    //↓ is original container metadata reset
    EXPECT_ANY_THROW(a.get<vec>()[0]);
    EXPECT_ANY_THROW(a.get<vec>()[1]);
    EXPECT_ANY_THROW(a.get<int>());
    //↓ is original container data reset
    EXPECT_EQ(a.data.capacity(), 0);
    //↓ are containers really dissociated
    a.insert_or_assign(56);
    EXPECT_EQ(a.data.size(), sizeof(int));
    EXPECT_EQ(b.get<int>(), 42);
}

TEST(HeterogeneousArray, reserve_construct)
{
    struct A { int a = 404; };
    struct B { int b = 42; };
    HeterogeneousArray c;
    c.reserve<A,B>();
    ASSERT_DEATH(c.insert(A{}), "");
    auto& a = c.construct(A{});
    auto& b = c.construct<B>();
    EXPECT_EQ(a.a, 404);
    EXPECT_EQ(b.b, 42);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
