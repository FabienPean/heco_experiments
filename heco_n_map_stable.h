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
#include <vector>

namespace heco
{
    struct HeterogeneousContainer_n
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
        template<typename T> inline auto type_id() { return TypeCounter::id<rm_cvref_t<T>>; }

        template<typename T>
        struct is_vector : public std::false_type {};
        template<typename... Args>
        struct is_vector<std::vector<Args...>> : public std::true_type {};
        template<typename T>
        constexpr static inline bool is_vector_v = is_vector<T>::value;

    public:
        HeterogeneousContainer_n() = default;
        HeterogeneousContainer_n(const HeterogeneousContainer_n&) = delete;
        HeterogeneousContainer_n& operator=(const HeterogeneousContainer_n&) = delete;
        HeterogeneousContainer_n(HeterogeneousContainer_n&&) = default;
        HeterogeneousContainer_n& operator=(HeterogeneousContainer_n&&) = default;
        ~HeterogeneousContainer_n() = default;

        using ptr_dtor = std::unique_ptr<void, void(*)(void*)>;
        std::unordered_map<type_id_t, ptr_dtor> data;

        template<typename T>
        auto vector() noexcept -> std::vector<T>& {
            return *static_cast<std::vector<T>*>(data.at(type_id<T>()).get());
        }

        template<typename T>
        auto vector(std::size_t i) noexcept -> T& {
            return (*static_cast<std::vector<T>*>(data.at(type_id<T>()).get()))[i];
        }

        template<typename Arg, typename... Args, typename = std::enable_if_t<!is_vector_v<Arg>>>
        bool insert(Arg&& arg, Args&&... args) {
            static_assert((std::is_same_v<Arg, Args> && ...));
            auto&& [it, b] = data.emplace(
                type_id<Arg>()
                , ptr_dtor(new std::vector{ std::forward<Arg>(arg),std::forward<Args>(args)... },
                    [](void* instance) { delete static_cast<std::vector<Arg>*>(instance); }));
            return b;
        }

        template<typename Arg, typename... Args>
        bool insert(const std::vector<Arg>& arg, const std::vector<Args>&... args) {
            static_assert((std::is_same_v<Arg, Args> && ...));
            std::vector<Arg> tmp(arg);
            auto&& [it, b] = data.emplace(
                type_id<Arg>()
                , ptr_dtor(new std::vector(arg),
                    [](void* instance) { delete static_cast<std::vector<Arg>*>(instance); }));
            auto&& v = it->second;
            (v.insert(v.end(), args.begin(), args.end()), ...);
            return b;
        }

    };
}