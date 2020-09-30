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
#include <memory>
#include <unordered_map>

namespace heco
{
    struct HeterogeneousContainer_SparseSet1;
    struct HeterogeneousContainer_SparseSet2;
    using HeterogeneousContainer_SparseSet = HeterogeneousContainer_SparseSet1;

    struct HeterogeneousContainer_SparseSet1
    {
    private:
        template<typename K, typename V, typename... Args>
        using map = std::unordered_map<K, V, Args...>;

        template<typename T>
        using rm_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;//remove_cvref_t from C++20 only

        using type_id_t = std::uint32_t;

        class TypeCounter {
            static inline type_id_t i = 0;
        public:
            template<typename T>
            static inline const auto id = i++;
        };
        template<typename T> static inline auto type_id() { return TypeCounter::id<rm_cvref_t<T>>; }
    
    public:
        HeterogeneousContainer_SparseSet1() = default;
        HeterogeneousContainer_SparseSet1(const HeterogeneousContainer_SparseSet1&) = delete;
        HeterogeneousContainer_SparseSet1& operator=(const HeterogeneousContainer_SparseSet1&) = delete;
        HeterogeneousContainer_SparseSet1(HeterogeneousContainer_SparseSet1&&) = default;
        HeterogeneousContainer_SparseSet1& operator=(HeterogeneousContainer_SparseSet1&&) = default;
        ~HeterogeneousContainer_SparseSet1() = default;

        using ptr_dtor = std::unique_ptr<void, void(*)(void*)>;
        struct any { type_id_t tag; ptr_dtor ptr; };
        using index = std::uint8_t;
        std::vector<index> sparse;
        std::vector<any> data;

        template<typename... Ts>
        bool contains() const noexcept { return ((sparse.size() > type_id<Ts>() && sparse[type_id<Ts>()] != index(-1)) && ...);}

        template<typename T>
        T* has() const noexcept { return contains<T>() ? &get<T>() : nullptr; }

        template<typename...Ts>
        void reserve() { reserve(sizeof...(Ts)); }
        void reserve(std::size_t n) { sparse.reserve(n); data.reserve(n); }

        template<typename T, typename... Rest>
        auto get() noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0)
                return *static_cast<U*>(data[sparse[type_id<U>()]].ptr.get());
            else
                return std::forward_as_tuple(get<T>(), get<Rest>()...);
        }

        template<typename T, typename... Rest>
        auto get() const noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0)
                return *static_cast<U*>(data[sparse[type_id<U>()]].ptr.get());
            else
                return std::forward_as_tuple(get<T>(), get<Rest>()...);
        }

        template<typename T>
        static constexpr bool must_be_copyable_if_lvalue = std::is_lvalue_reference_v<T> && std::is_copy_constructible_v<T>;
        template<typename T>
        static constexpr bool must_be_moveable_if_rvalue = (std::is_object_v<T> || std::is_rvalue_reference_v<T>) && std::is_move_constructible_v<T>;

        template<typename T = void, typename... Args>
        auto insert(Args&& ... args) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, void>)
                static_assert(((must_be_copyable_if_lvalue<Args> || must_be_moveable_if_rvalue<Args>) && ...), "Use in-place version insert<T>(Args...) instead");

            if constexpr (!std::is_same_v<T, void>)
                return insert_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return insert_1<Args...>(std::forward<Args>(args)...);
            else
                return insert_n(std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        auto insert_1(Args&& ... args) -> decltype(auto)
        {
            using U = rm_cvref_t<T>;
            auto id = type_id<T>();
            auto&& it = data.emplace_back(any{ id, std::unique_ptr<void, void(*)(void*)>{ new U{ std::forward<Args>(args)... }, +[](void* instance) { delete static_cast<U*>(instance); } } });
            if (id >= sparse.size()) 
                sparse.resize(id+1, -1);
            sparse[id] = data.size()-1;
            return *static_cast<U*>(it.ptr.get());
        }

        template<typename... Ts>
        auto insert_n(Ts&& ... values) -> decltype(auto)
        {
            data.reserve(data.size() + sizeof...(Ts));
            return std::forward_as_tuple(insert_1<Ts>(std::forward<Ts>(values))...);
        }

        template<typename T = void, typename... Args>
        decltype(auto) insert_or_assign(Args&& ... args)
        {
            if constexpr (!std::is_same_v<T, void>)
                return insert_or_assign_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return insert_or_assign_1<Args...>(std::forward<Args>(args)...);
            else
                return std::forward_as_tuple(insert_or_assign_1<Args>(std::forward<Args>(args))...);
        }

        template<typename T, typename... Args>
        decltype(auto) insert_or_assign_1(Args&& ... args)
        {
            using U = rm_cvref_t<T>;
            if (auto p = has<T>()) {
                *p = U(std::forward<Args>(args)...);
                return *p;
            }
            else {
                return insert_1<T>(std::forward<Args>(args)...);
            }
        }
    };

    struct HeterogeneousContainer_SparseSet2
    {
    private:
        template<typename K, typename V, typename... Args>
        using map = std::unordered_map<K, V, Args...>;

        template<typename T>
        using rm_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;//remove_cvref_t from C++20 only

        using type_id_t = std::uint32_t;

        class TypeCounter {
            static inline type_id_t i = 0;
        public:
            template<typename T>
            static inline const auto id = i++;
        };
        template<typename T> static inline auto type_id() { return TypeCounter::id<rm_cvref_t<T>>; }

    public:
        HeterogeneousContainer_SparseSet2() = default;
        HeterogeneousContainer_SparseSet2(const HeterogeneousContainer_SparseSet&) = delete;
        HeterogeneousContainer_SparseSet2& operator=(const HeterogeneousContainer_SparseSet2&) = delete;
        HeterogeneousContainer_SparseSet2(HeterogeneousContainer_SparseSet2&&) = default;
        HeterogeneousContainer_SparseSet2& operator=(HeterogeneousContainer_SparseSet2&&) = default;
        ~HeterogeneousContainer_SparseSet2() = default;

        using ptr_dtor = std::unique_ptr<void, void(*)(void*)>;
        std::vector<std::uint8_t> sparse;
        std::vector<type_id_t> tags;
        std::vector<ptr_dtor> data;


        template<typename... Ts>
        bool contains() const noexcept { return ((sparse.size() > type_id<Ts>() && sparse[type_id<Ts>()] != -1) && ...); }

        template<typename T>
        T* has() const noexcept { return contains<T>() ? &get<T>() : nullptr; }

        template<typename...Ts>
        void reserve() { reserve(sizeof...(Ts)); }
        void reserve(std::size_t n) { sparse.reserve(n); tags.reserve(n); data.reserve(n); }

        template<typename T, typename... Rest>
        auto get() noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0)
                return *static_cast<U*>(data[sparse[type_id<U>()]].get());
            else
                return std::forward_as_tuple(get<T>(), get<Rest>()...);
        }

        template<typename T, typename... Rest>
        auto get() const noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0)
                return *static_cast<U*>(data[sparse[type_id<U>()]].get());
            else
                return std::forward_as_tuple(get<T>(), get<Rest>()...);
        }

        template<typename T>
        static constexpr bool must_be_copyable_if_lvalue = std::is_lvalue_reference_v<T> && std::is_copy_constructible_v<T>;
        template<typename T>
        static constexpr bool must_be_moveable_if_rvalue = (std::is_object_v<T> || std::is_rvalue_reference_v<T>) && std::is_move_constructible_v<T>;

        template<typename T = void, typename... Args>
        auto insert(Args&& ... args) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, void>)
                static_assert(((must_be_copyable_if_lvalue<Args> || must_be_moveable_if_rvalue<Args>) && ...), "Use in-place version insert<T>(Args...) instead");

            if constexpr (!std::is_same_v<T, void>)
                return insert_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return insert_1<Args...>(std::forward<Args>(args)...);
            else
                return insert_n(std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        auto insert_1(Args&& ... args) -> decltype(auto)
        {
            using U = rm_cvref_t<T>;
            auto id = type_id<T>();
            auto&& it = data.emplace_back(ptr_dtor{ new U{ std::forward<Args>(args)... }, +[](void* instance) { delete static_cast<U*>(instance); } } );
            tags.emplace_back(id);
            if (id >= sparse.size())
                sparse.resize(id + 1, -1);
            sparse[id] = data.size() - 1;
            return *static_cast<U*>(it.get());
        }

        template<typename... Ts>
        auto insert_n(Ts&& ... values) -> decltype(auto)
        {
            data.reserve(data.size() + sizeof...(Ts));
            return std::forward_as_tuple(insert_1<Ts>(std::forward<Ts>(values))...);
        }

        template<typename T = void, typename... Args>
        decltype(auto) insert_or_assign(Args&& ... args)
        {
            if constexpr (!std::is_same_v<T, void>)
                return insert_or_assign_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return insert_or_assign_1<Args...>(std::forward<Args>(args)...);
            else
                return std::forward_as_tuple(insert_or_assign_1<Args>(std::forward<Args>(args))...);
        }

        template<typename T, typename... Args>
        decltype(auto) insert_or_assign_1(Args&& ... args)
        {
            using U = rm_cvref_t<T>;
            if (auto p = has<T>()) {
                *p = U(std::forward<Args>(args)...);
                return *p;
            }
            else {
                return insert_1<T>(std::forward<Args>(args)...);
            }
        }
    };
}