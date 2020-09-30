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
#include <memory_resource>

namespace heco
{
#include<memory>
#include<cmath>
#include<cassert>
#include<iostream>
    using typeid_t = void(*)(void*);

    template<typename T>
    void destroy_at(void* instance) {
        std::destroy_at(static_cast<T*>(instance));
    }

    template<typename T>
    typeid_t get_typeid() { return destroy_at<T>; }

    template<typename T>
    typeid_t make_dtor(void* start = nullptr, void* end = nullptr) {
        if (!std::is_trivially_destructible_v<T> && end == nullptr && start != nullptr)
            destroy_at<T>(start);
        if (!std::is_trivially_destructible_v<T> && end != nullptr && start != nullptr && end >= start)
            for (auto it = start; it < end; it = static_cast<T*>(it) + sizeof(T))
                destroy_at<T>(it);
        return destroy_at<T>;
    }

    template<typename T>
    struct te_vector;

    struct te_vector_base {
        using fp = typeid_t(*)(void*, void*);
        fp  dtor = nullptr;
        void* _sta = nullptr;
        void* _end = nullptr;
        void* _cap = nullptr;

        template<typename T>
        static te_vector_base create() {
            return te_vector<T>();
        }

        te_vector_base()
        {}
        te_vector_base(fp arg) :
            dtor(arg)
        {}
        te_vector_base(const te_vector_base&) = delete;
        te_vector_base& operator=(const te_vector_base&) = delete;
        te_vector_base(te_vector_base&& other)
        {
            // dtor = std::exchange(other.dtor, nullptr);//why?
            dtor = other.dtor;
            _sta = std::exchange(other._sta, nullptr);
            _end = std::exchange(other._end, nullptr);
            _cap = std::exchange(other._cap, nullptr);
        }
        te_vector_base& operator=(te_vector_base&& other) {
            if (this != &other) {
                dtor = other.dtor;
                _sta = std::exchange(other._sta, nullptr);
                _end = std::exchange(other._end, nullptr);
                _cap = std::exchange(other._cap, nullptr);
            }
            return *this;
        };

        ~te_vector_base() noexcept {
            if (dtor != nullptr && _sta != nullptr)
                dtor(_sta, _end);
            if (_sta != nullptr)
                free(_sta);
        }

        typeid_t get_typeid() { return dtor(nullptr, nullptr); }

        template<typename T>
        auto& vector() {
            assert(::get_typeid<T>() == get_typeid());
            return *std::launder(reinterpret_cast<te_vector<T>*>(this));
        }
        template<typename T>
        auto& vector(size_t i) {
            assert(::get_typeid<T>() == get_typeid());
            return (*std::launder(reinterpret_cast<te_vector<T>*>(this)))[i];
        }
    };

    template<typename T>
    struct te_vector : te_vector_base {
        static constexpr auto SIZE = sizeof(T);
        size_t capacity() { return cap() - begin(); }
        size_t size() { return end() - begin(); }

        T* cap() { return static_cast<T*>(_cap); }
        T* begin() { return static_cast<T*>(_sta); }
        T* end() { return static_cast<T*>(_end); }

        te_vector() : te_vector_base(make_dtor<T>)
        {}

        auto resize(size_t n) {
            using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
            T* new_start(reinterpret_cast<T*>(new Storage[n]));
            _end = std::uninitialized_move(begin(), end(), new_start);
            _sta = new_start;
        }

        static constexpr auto growth_factor = 1.5;
        T& push_back(T&& arg) {
            if (size() >= capacity()) {
                size_t n = std::floor((capacity() + 1) * growth_factor);
                resize(n);
                _cap = begin() + n;
            }
            T* obj = new(_end) T(arg);
            _end = end() + 1;
            return *obj;
        }

        T& operator[](size_t i) { return *(begin() + i); }
    };


    /*auto test(te_vector_base&& vin) {
        te_vector_base v; v = std::move(vin);
        assert(v.vector<int>(0) == 42);
    }
    int main() {
        te_vector_base vd = te_vector_base::create<int>();
        vd.vector<int>().push_back(42);
        assert(vd.vector<int>(0) == 42);
        vd.vector<int>().push_back(3);
        assert(vd.vector<int>(0) == 42);
        assert(vd.vector<int>(1) == 3);
        test(std::move(vd));
        vd.vector<int>().push_back(23);
        assert(vd.vector<int>(0) == 23);
        vd.vector<int>().push_back(2);
        assert(vd.vector<int>(0) == 23);
        assert(vd.vector<int>(1) == 2);

        return vd.vector<int>().size();
    }*/

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

        std::unordered_map<type_id_t, te_vector> data;

        template<typename T>
        auto vector() noexcept -> std::vector<T>& {
            return *std::launder(reinterpret_cast<std::vector<T>*>(data.at(type_id<T>())));
        }

        template<typename T>
        auto vector(std::size_t i) noexcept -> T& {
            return (*std::launder(reinterpret_cast<std::vector<T>*>(data.at(type_id<T>()))))[i];
        }

        template<typename Arg, typename... Args, typename = std::enable_if_t<!is_vector_v<Arg>>>
        bool insert(Arg&& arg, Args&&... args) {
            static_assert((std::is_same_v<Arg, Args> && ...));
            return false;
        }

        template<typename Arg, typename... Args>
        bool insert(const std::vector<Arg>& arg, const std::vector<Args>&... args) {
            static_assert((std::is_same_v<Arg, Args> && ...));
            return false;
        }

    };
}