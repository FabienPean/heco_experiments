// MIT License
//
// Copyright(c) 2020 Fabien Péan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include <cassert>
#include <cstddef>
#include <array>
#include <vector>
#include <algorithm>
#include <numeric>
#include <boost/align/aligned_allocator.hpp>
#include <boost/container/static_vector.hpp>
#include <unordered_map>

namespace heco {
    template<typename T, size_t N>
    using aligned_allocator = boost::alignment::aligned_allocator<T, N>;

    template<typename T, size_t N>
    using vector_fixed_capacity = boost::container::static_vector<T, N>;

    template<typename K, typename V, typename... Args>
    using map = std::unordered_map<K, V, Args...>;

    template<typename T>
    using rm_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;//remove_cvref_t from C++20 only

    template<typename T = void, typename... Ts>
    constexpr bool all_types_different = !std::disjunction_v<std::is_same<T, Ts>...> && (sizeof...(Ts) > 0) ? all_types_different<Ts...> : true;

    using type_id_t = std::uint32_t;

    class TypeCounter {
        static inline type_id_t i = 0;
    public:
        template<typename T>
        static inline const auto id = i++;
    };
    template<typename T> inline auto type_id() { return TypeCounter::id<rm_cvref_t<T>>; }

    template<typename T, std::size_t N>
    struct array : std::array<T, N> {
        template<size_t M = N, typename = std::enable_if_t<M == 1>>//https://stackoverflow.com/questions/18100297/how-can-i-use-stdenable-if-in-a-conversion-operator | https://godbolt.org/z/PHVy-L
        constexpr operator const T& () const { return (*this)[0]; }
        template<size_t M = N, typename = std::enable_if_t<M == 1>>//https://stackoverflow.com/questions/18100297/how-can-i-use-stdenable-if-in-a-conversion-operator | https://godbolt.org/z/PHVy-L
        constexpr operator T& () { return (*this)[0]; }
    };
    template<typename T, typename... Ts, typename = std::enable_if_t<(std::is_same_v<T, Ts>&& ...)>>
    array(T t, Ts... ts)->array<T, 1 + sizeof...(Ts)>;//template deduction guide needed

    constexpr size_t default_alignment = 64;

    class HeterogeneousArray
    {
    public:
        using This = HeterogeneousArray;
        using offset_t = std::uint32_t;
        using destructor_t = void(*)(This&);

        HeterogeneousArray() = default;
        HeterogeneousArray(const HeterogeneousArray&) = delete;
        HeterogeneousArray& operator=(const HeterogeneousArray&) = delete;
        HeterogeneousArray(HeterogeneousArray&&) = default;
        HeterogeneousArray& operator=(HeterogeneousArray&&) = default;
        ~HeterogeneousArray() {
            for (auto [tid, destructor] : destructors)
                destructor(*this);
        }

    private:
        map<type_id_t, offset_t> offsets;
        mutable std::vector<std::byte, aligned_allocator<std::byte, default_alignment>> data;
        map<type_id_t, destructor_t> destructors;

    public:
        bool is_allocated(const type_id_t& type) const { return offsets.count(type); }
        template<typename T>
        bool is_allocated() const { return is_allocated(type_id<T>()); }

        bool is_constructed(const type_id_t& type) const { return destructors.count(type); }
        template<typename T>
        bool is_constructed() const { return is_constructed(type_id<T>()); }

        bool contains(const type_id_t& type) const { return is_constructed(type); }
        template<typename... Ts>
        bool contains() const { return (contains(type_id<Ts>()) && ...); }

        template<typename T, typename U = rm_cvref_t<T>>
        auto has() const {
            const type_id_t tid = type_id<U>();
            if constexpr (std::is_empty_v<U>)
                return contains(tid);
            else {
                if (contains(tid))
                    return &get<U>(offsets.at(tid));
                else
                    return (U*)nullptr;
            }
        }

        template<typename T, typename... Rest>
        auto offset_of() const
        {
            return array{ offsets.at(type_id<T>()), offsets.at(type_id<Rest>())... };
        }

        template<typename T, typename... Rest>
        auto id_of() const
        {
            return array{ type_id<T>(), type_id<Rest>()... };
        }

        template<typename... Ts>
        decltype(auto) get() const noexcept
        {
            static_assert(sizeof...(Ts) > 0);
            return get<Ts...>(offsets.at(type_id<Ts>()) ...);
        }

        template<typename T, typename... Rest, typename Id, typename... Ids>
        decltype(auto) get(Id off, Ids... offs) const noexcept
        {
            static_assert(sizeof...(Rest) == sizeof...(Ids));
            static_assert(std::is_convertible_v<Id, offset_t> && (std::is_convertible_v<Ids, offset_t> && ...));
            assert(offsets.at(type_id<T>()) == off && ((offsets.at(type_id<Rest>()) == offs) && ...));
            auto*const p = data.data();
            if constexpr (sizeof...(Rest) == 0)
                return do_get<T>(off);
            else
                return std::forward_as_tuple( do_get<T>(off), do_get<Rest>(offs)... );
        }

        template<typename... Ts, size_t N>
        decltype(auto) get(const array<offset_t,N>& offs) const noexcept
        {
            static_assert(sizeof...(Ts) == N);
            if constexpr (sizeof...(Ts) == 1)
                return (do_get<Ts>(offs[0]),...);
            else {
                //return std::apply([&](auto... off) {return get<Ts...>(off...); }, offs);
                unsigned i = -1; return std::tuple<Ts&...>{ do_get<Ts>(offs[++i])... };
            }
        }

        template<typename... Ts>
        auto reserve()
        {
            constexpr auto N = sizeof...(Ts);
            assert((!is_allocated<Ts>() && ...));
            const auto offsets_to_insert = do_allocate<Ts...>();
            offsets.reserve(offsets.size() + N);
            destructors.reserve(destructors.size() + N);
            unsigned i = 0;
            (offsets.emplace(type_id<Ts>(), offsets_to_insert[i++]), ...);
            if constexpr (sizeof...(Ts) == 1)
                return offsets_to_insert[0];
            else
                return offsets_to_insert;
        }

        void reserve(size_t n_bytes, size_t n_types = 0, size_t n_destructors = 0)
        {
            data.reserve(n_bytes);
            offsets.reserve(n_types);
            destructors.reserve(n_destructors);
        }

        template<typename T = void, typename... Args>
        decltype(auto) construct(Args&& ... args) 
        {
            if constexpr (std::is_same_v<T, void>)
                static_assert(((must_be_copyable_if_lvalue<Args> || must_be_moveable_if_rvalue<Args>) && ...), "Use in-place version insert<T>(Args...) instead");

            if constexpr (!std::is_same_v<T, void>)
                return construct_1<T>(offset_of<T>(), std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return (construct_1<Args>(offset_of<Args>(), std::forward<Args>(args)), ...);
            else
                return construct_n(offset_of<Args...>(), std::forward<Args>(args)...);
        }

        template<typename T = void, typename... Args>
        decltype(auto) insert(Args&& ... args) 
        {
            if constexpr (std::is_same_v<T, void>)
                static_assert(((must_be_copyable_if_lvalue<Args> || must_be_moveable_if_rvalue<Args>) && ...), "Use in-place version insert<T>(Args...) instead");

            if constexpr (!std::is_same_v<T, void>)
                return insert_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return (insert_1<Args>(std::forward<Args>(args)), ...);
            else
                return insert_n(std::forward<Args>(args)...);
        }

        template<typename T = void, typename... Args>
        decltype(auto) assign(Args&&... args) 
        {
            if constexpr (!std::is_same_v<T, void>)
                return assign_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return (assign_1<Args>(std::forward<Args>(args)), ...);
            else
                return assign_n(std::forward<Args>(args)...);
        }

        template<typename T = void, typename... Args>
        decltype(auto) insert_or_assign(Args&& ... args)
        {
            if constexpr (!std::is_same_v<T, void>)
                return insert_or_assign_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return (insert_or_assign_1<Args>(std::forward<Args>(args)), ...);
            else
                return std::tuple<rm_cvref_t<Args>& ...>{insert_or_assign_1<Args>(std::forward<Args>(args))...};
        }

        template<typename... Ts>
        void destruct() {
            (do_destruct<Ts>(), ...);
        }

        void clear()
        {
            for (auto [tid, destructor] : destructors)
                destructor(*this);
            destructors.clear();
            offsets.clear();
            data.clear();
        }

    private:
        template<typename T>
        static constexpr bool must_be_copyable_if_lvalue = std::is_lvalue_reference_v<T> && std::is_copy_constructible_v<T>;
        template<typename T>
        static constexpr bool must_be_moveable_if_rvalue = (std::is_object_v<T> || std::is_rvalue_reference_v<T>) && std::is_move_constructible_v<T>;

        template<typename T>
        void record_type(offset_t to_add) { offsets.emplace(type_id<T>(), to_add); }
        void record_type(const type_id_t& type, offset_t to_add) { offsets.emplace(type, to_add); }

        template<typename T, typename U = rm_cvref_t<T>>
        void record_dtor(const type_id_t& type) {
            if constexpr (std::is_empty_v<U>)
                destructors.emplace(type, +[](This& c) {});
            else
                destructors.emplace(type, +[](This& c) { std::destroy_at(&c.get<U>()); });
        }
        template<typename T, typename U = rm_cvref_t<T>>
        void record_dtor() { record_dtor<U>(type_id<U>()); }

        template<typename T, typename... Args>
        decltype(auto) construct_1(offset_t offset, Args&& ... args) 
        {
            assert(!is_constructed<T>());
            record_dtor<T>();
            return do_construct<T>(offset, std::forward<Args>(args)...);
        }

        template<typename... Ts, std::size_t N = sizeof...(Ts)>
        decltype(auto) construct_n(const std::array<offset_t, N>& offs, Ts&&... args) 
        {
            static_assert(sizeof...(Ts) > 1);
            unsigned i = 0;
            return std::tuple<rm_cvref_t<Ts>&...>{construct_1<Ts>(offs[i++], std::forward<Ts>(args))...};
        }

        template<typename T, typename... Args>
        decltype(auto) insert_1(Args&& ... args) 
        {
            if constexpr (std::is_empty_v<T>) {
                const auto tid = type_id<T>();
                record_type(tid, 0);
                record_dtor<T>(tid);
                return;
            }
            else {
                assert(!contains<T>());
                const offset_t n = reserve<T>();
                return construct_1<T>(n, std::forward<Args>(args)...);
            }
        }

        template<typename... Ts>
        decltype(auto) insert_n(Ts&& ... values) 
        {
            constexpr bool all_empty = (std::is_empty_v<Ts> && ...);
            constexpr bool none_empty = (!std::is_empty_v<Ts> && ...);
            static_assert(all_empty || none_empty, "Cannot insert a mix of empty and non-empty types.");

            constexpr size_t N = sizeof...(Ts);
            const type_id_t type_index[N] = { type_id<Ts>()... };
            if constexpr (all_empty) {
                for (auto ti : type_index)
                    record_type(ti, 0);
                size_t i = 0;
                (record_dtor<Ts>(type_index[i++]), ...);
                return;
            }
            else {
                assert((!contains<Ts>() && ...));
                auto new_offsets = reserve<Ts...>();
                return construct_n(new_offsets, std::forward<Ts>(values)...);
            }
        }

        template<typename T, typename... Args>
        decltype(auto) assign_1(Args&& ... args) 
        {
            if (!is_constructed<T>())
                return construct_1<T>(offsets[type_id<T>()], std::forward<Args>(args)...);
            else
                return do_assign<T>(offsets[type_id<T>()], std::forward<Args>(args)...);
        }

        template<typename... Ts>
        decltype(auto) assign_n(Ts&& ... args) 
        {
            static_assert(sizeof...(Ts) > 1);
            return std::tuple<rm_cvref_t<Ts>&...>{assign_1<Ts>(std::forward<Ts>(args))...};
        }

        template<typename T, typename... Args>
        decltype(auto) insert_or_assign_1(Args&& ... args) 
        {
            if (!is_allocated<T>())
                return insert<T>(std::forward<Args>(args)...);
            else
                return assign<T>(std::forward<Args>(args)...);
        }

        template<typename... Ts>
        decltype(auto) insert_or_assign_n(Ts&& ... args) 
        {
            static_assert(sizeof...(Ts) > 1);
            return std::tuple<rm_cvref_t<Ts>&...>{insert_or_assign_1<Ts>(std::forward<Ts>(args))...};
        }

        template<typename T>
        void do_destruct()
        {
            assert(contains<T>());
            const auto type_index = type_id<T>();
            if constexpr (!std::is_trivially_destructible_v<T>)
                std::invoke(destructors.at(type_index), *this);
            destructors.erase(type_index);
        }

        template<typename T, typename U = rm_cvref_t<T>>
        T& do_get(offset_t n) const noexcept
        {
            static_assert(!std::is_empty_v<U>);
            return *std::launder(reinterpret_cast<U*>(&data[n]));
        }

        template<typename T, typename U = rm_cvref_t<T>>
        T& do_get(void* p) const noexcept
        {
            static_assert(!std::is_empty_v<U>);
            return *std::launder(reinterpret_cast<U*>(p));
        }

        template<typename T, typename... Args, typename U = rm_cvref_t<T>>
        U& do_construct(offset_t n, Args&& ... args) 
        {
            static_assert(!std::is_empty_v<U>);
            U* value = new(&data[n]) U{ std::forward<Args>(args)... };
            return *value;
        }

        template<typename T, typename... Args, typename U = rm_cvref_t<T>>
        U& do_assign(offset_t n, Args&&... args) 
        {
            static_assert(!std::is_empty_v<U>);
            if constexpr (sizeof...(Args) == 1 && (std::is_same_v<U, rm_cvref_t<Args>> && ...))
                return do_get<U>(n) = (std::forward<Args>(args...), ...);
            else
                return do_get<U>(n) = U{ std::forward<Args>(args)... };
        }

        template<typename... Ts>
        auto do_allocate() 
        {
            static_assert(((alignof(Ts) <= default_alignment) && ...));
            static_assert(all_types_different<rm_cvref_t<Ts>...>);
            if constexpr (sizeof...(Ts) == 1)
                return do_allocate_1<Ts...>();
            else
                return do_allocate_n<Ts...>();
        }

        template<typename T>
        auto do_allocate_1() ->std::array<offset_t, 1>
        {
            using namespace std;
            constexpr size_t alignment = alignof(T);
            constexpr size_t size = sizeof(T);
            const size_t n = data.size();
            const uintptr_t ptr_end = uintptr_t(data.data() + n);
            const size_t padding = ((~ptr_end + 1) & (alignment - 1));
            data.resize(n + padding + size);
            return array{ offset_t(n + padding) };
        }

        template<typename... Ts>
        auto do_allocate_n() ->std::array<offset_t, sizeof...(Ts)>
        {
            using namespace std;

            constexpr size_t N = sizeof...(Ts);
            array<offset_t, N> output;
            size_t to_allocate = 0;

            const size_t size_before = data.size();
            uintptr_t ptr_end = uintptr_t(data.data() + size_before);

            constexpr array<size_t, N> alignments = { alignof(Ts)... };
            constexpr array<size_t, N> sizes = { sizeof(Ts)... };
            array<size_t, N> paddings = {};
            array<size_t, N> min_paddings = {};
            size_t n_min = 0;

            vector_fixed_capacity<size_t, N> indices;
            indices.resize(N);
            iota(indices.begin(), indices.end(), 0);
            //^ Fill container with indices to visit [0,1,...,N-1]
            while (!indices.empty())
            {
                for_each(begin(indices), end(indices), [&](auto i) { paddings[i] = ((~ptr_end + 1) & (alignments[i] - 1)); });
                //^ Fill current padding for all remaining types to allocate for
                size_t min_padding = paddings[*min_element(begin(indices), end(indices), [&](auto i, auto j) {return paddings[i] < paddings[j]; })];
                //^ Get value of minimum padding
                n_min = 0;
                for (auto id : indices) {
                    if (paddings[id] == min_padding)
                        min_paddings[n_min++] = id;//< Store index of all arguments with same padding value
                }
                size_t id_element = *max_element(begin(min_paddings), begin(min_paddings) + n_min, [&](auto i, auto j) {return sizes[i] < sizes[j]; });
                //^ Get index of type to store that has maximum size for the minimum padding
                output[id_element] = size_before + to_allocate + min_padding;
                to_allocate += min_padding + sizes[id_element];
                ptr_end += min_padding + sizes[id_element];
                indices.erase(find(begin(indices), end(indices), id_element));
            }
            size_t size_after = size_before + to_allocate;
            data.resize(size_after);
            return output;
        }
    };
}