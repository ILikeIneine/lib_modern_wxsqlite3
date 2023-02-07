#pragma once

#include <type_traits>
#include <tuple>
#include <vector>

namespace sqlite
{

// value traits
template <typename Type>
struct is_sqlite_value : public std::integral_constant<
        bool,
        std::is_floating_point<Type>::value
        || std::is_integral<Type>::value
        || std::is_same<std::string, Type>::value
        || std::is_same<sqlite3_int64, Type>::value
        > { };

template <typename Type, typename Allocator>
struct is_sqlite_value<std::vector<Type, Allocator>> : public std::integral_constant<
        bool,
        std::is_floating_point<Type>::value
        || std::is_integral<Type>::value
        || std::is_same<sqlite3_int64, Type>::value
        > { };


// functor traits

template <typename> struct function_traits_impl{};

template <typename Function>
struct function_traits
    : public function_traits<decltype(&std::remove_reference_t<Function>::operator())> {
};

/* const functors */
template <
    typename ReturnType,
    typename ClassType,
    typename... Args>
struct function_traits<
    ReturnType (ClassType::*)(Args...) const>
    : function_traits_impl<ReturnType (*)(Args...)> {
};

/* the non-const operator() support 
 *  work with user-defined functors */
template <
    typename ReturnType,
    typename ClassType,
    typename... Args>
struct function_traits<
    ReturnType (ClassType::*)(Args...)>
    : function_traits_impl<ReturnType (*)(Args...)> {
};

    
template <
    typename ReturnType,
    typename... Args>
struct function_traits_impl<ReturnType (*)(Args...)> 
{
    using result_type = ReturnType;

    template <std::size_t Idx>
    using arg_type = std::tuple_element_t<Idx, std::tuple<Args...>>;

    static constexpr std::size_t arity = sizeof...(Args);
};


} // namespace sqlite
