#pragma once
#if defined(_LIBCPP_VERSION) && ! defined(_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
#include <functional>
namespace std
{
template <typename A, typename R> struct unary_function
{
    using argument_type = A;
    using result_type = R;
};
template <typename A1, typename A2, typename R> struct binary_function
{
    using first_argument_type = A1;
    using second_argument_type = A2;
    using result_type = R;
};
}
#endif
