#pragma once

#include "updater/updater.hpp"
#include "traits/base_dic.hpp"
#include "traits/has_type.hpp"
#include "traits/without_duplicates.hpp"

#include <tao/tuple/tuple.hpp>


template <typename... vectors>
struct scheme;

template <typename... vectors>
struct scheme_store;


namespace detail
{
    template <typename component, typename... Args>
    struct scheme_arguments
    {
        component& comp;
        tao::tuple<Args...> args;
    };
}


template <typename... vectors>
struct scheme_store
{
    template <typename T> using dic_t = typename base_dic<T, tao::tuple<vectors...>>::type;

    constexpr scheme_store()
    {}

    template <typename T>
    constexpr inline T& get()
    {
        return tao::get<T>(components);
    }

    tao::tuple<vectors...> components;
};

template <typename... vectors>
class scheme
{
    template <typename... T> friend class scheme;

public:
    tao::tuple<std::add_lvalue_reference_t<vectors>...> components;

    constexpr updater<std::add_pointer_t<vectors>...> make_updater(bool contiguous_component_execution)
    {
        return updater<std::add_pointer_t<vectors>...>(contiguous_component_execution, components);
    }

    template <typename T>
    constexpr inline T& get()
    {
        return tao::get<T&>(components);
    }

    template <typename T>
    constexpr inline bool has() const
    {
        return has_type<T, tao::tuple<vectors...>>::value;
    }

    template <typename T>
    constexpr inline void require() const
    {
        static_assert(has_type<T, tao::tuple<vectors...>>::value, "Requirement not met");
    }

    template <typename T, typename... Args>
    constexpr auto args(Args&&... args) -> detail::scheme_arguments<std::add_lvalue_reference_t<typename base_dic<T, tao::tuple<vectors...>>::type>, std::decay_t<Args>...>
    {
        using D = typename base_dic<T, tao::tuple<vectors...>>::type;
        require<D>();

        return {
            .comp = tao::get<std::add_lvalue_reference_t<D>>(components),
            .args = tao::make_tuple(std::forward<std::decay_t<Args>>(args)...)
        };
    }

    template <typename... T, typename... D>
    constexpr auto overlap(scheme_store<T...>& store, scheme<D...>& other)
    {
        using W = without_duplicates<scheme, scheme<D..., vectors...>>;
        return W{ store };
    }

    template <typename... T>
    static constexpr inline auto make(scheme_store<T...>& store)
    {
        using W = without_duplicates<scheme, scheme<typename scheme_store<T...>::template dic_t<vectors>...>>;
        return W{ store };
    }

private:
    template <typename... T>
    constexpr scheme(scheme_store<T...>& store) :
        components(store.template get<vectors>()...)
    {}
};


template <typename... T, typename A, typename B, typename... O>
constexpr inline auto overlap(scheme_store<T...>& store, A&& a, B&& b, O&&... other)
{
    if constexpr (sizeof...(other) == 0)
    {
        return a.overlap(store, b);
    }
    else
    {
        return overlap(store, a.overlap(b), std::forward<O>(other)...);
    }
}
