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
#include <cstddef>        // for size_t
#include <cstdint>        // for std::uint32_t
#include <memory>         // for unique_ptr
#include <tuple>          // for forward_as_tuple
#include <type_traits>    // for remove_reference_t, remove_cv_t
#include <unordered_map>  // for unordered_map
#include <utility>        // for forward

namespace heco
{
    struct HeterogeneousContainer
    {
    private:
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
        HeterogeneousContainer() = default;
        HeterogeneousContainer(const HeterogeneousContainer&) = delete;
        HeterogeneousContainer& operator=(const HeterogeneousContainer&) = delete;
        HeterogeneousContainer(HeterogeneousContainer&&) = default;
        HeterogeneousContainer& operator=(HeterogeneousContainer&&) = default;
        ~HeterogeneousContainer() = default;

        using ptr_dtor = std::unique_ptr<void,void(*)(void*)>;
        std::unordered_map<type_id_t, ptr_dtor> data;

        template<typename... Ts>
        bool contains() const noexcept { return (data.count(type_id<Ts>()) && ...);}

        template<typename...Ts>
        void reserve() { reserve(sizeof...(Ts)); }
        void reserve(std::size_t n) { data.reserve(n); }

        template<typename T, typename... Rest>
        auto has() noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0) {
                auto it = data.find(type_id<U>());
                return (it != data.cend()) ? static_cast<U*>(it->second.get()) : nullptr;
            }
            else
                return std::forward_as_tuple(has<T>(), has<Rest>()...);
        }

        template<typename T, typename... Rest>
        auto get() noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0)
                return *static_cast<U*>(data.at(type_id<U>()).get());
            else
                return std::forward_as_tuple(get<T>(), get<Rest>()...);
        }

        template<typename T, typename... Rest>
        auto get() const noexcept -> decltype(auto)
        {
            using U = std::remove_reference_t<T>;
            if constexpr (sizeof...(Rest) == 0)
                return *static_cast<U*>(data.at(type_id<U>()).get());
            else
                return std::forward_as_tuple(get<T>(), get<Rest>()...);
        }

        template<typename T = void, typename... Args>
        auto insert(Args&& ... args) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, void>)
                static_assert(((copyable_if_lvalue<Args> || moveable_if_rvalue<Args>) && ...)
                    , "Use in-place construction version insert<T>(Args...) instead.");

            if constexpr (!std::is_same_v<T, void>)
                return insert_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return insert_1<Args...>(std::forward<Args>(args)...);
            else
                return insert_n(std::forward<Args>(args)...);
        }

        template<typename T = void, typename... Args>
        auto insert_or_assign(Args&& ... args) -> decltype(auto)
        {
            if constexpr (!std::is_same_v<T, void>)
                return insert_or_assign_1<T>(std::forward<Args>(args)...);
            else if constexpr (sizeof...(Args) == 1)
                return insert_or_assign_1<Args...>(std::forward<Args>(args)...);
            else
                return std::forward_as_tuple(insert_or_assign_1<Args>(std::forward<Args>(args))...);
        }

    private:
        template<typename T>
        static constexpr bool copyable_if_lvalue = std::is_lvalue_reference_v<T> && std::is_copy_constructible_v<T>;
        template<typename T>
        static constexpr bool moveable_if_rvalue = (std::is_object_v<T> || std::is_rvalue_reference_v<T>) && std::is_move_constructible_v<T>;

        template<typename T, typename... Args>
         auto insert_1(Args&&... args) -> decltype(auto)
        {
            using U = rm_cvref_t<T>;
            auto&&[it, in] = data.emplace(
                type_id<T>(),
                std::unique_ptr<void, void (*)(void*)>{ new U{ std::forward<Args>(args)... },
                                                        +[](void* instance) { delete static_cast<U*>(instance); } });
            return *static_cast<U*>(it->second.get());
        }

         template<typename... Ts>
         auto insert_n(Ts&&... values) -> decltype(auto)
         {
             data.reserve(data.size() + sizeof...(Ts));
             return std::forward_as_tuple(insert_1<Ts>(std::forward<Ts>(values))...);
         }

        template<typename T, typename... Args>
        auto insert_or_assign_1(Args&& ... args) -> decltype(auto)
        {
            using U = rm_cvref_t<T>;
            auto&&[it,in] = data.insert_or_assign(
                type_id<T>(),
                std::unique_ptr<void, void(*)(void*)>{ new U{ std::forward<Args>(args)... },
                                                       +[](void* instance) { delete static_cast<U*>(instance); } }
            );
            return *static_cast<U*>(it->second.get());
        }
    };
}