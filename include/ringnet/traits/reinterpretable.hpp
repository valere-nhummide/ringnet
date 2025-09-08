#pragma once

#include <type_traits>

namespace ringnet::traits
{
template <typename T>
using is_safe_for_reinterpret_cast = std::conjunction<std::is_trivially_copyable<T>, std::is_standard_layout<T>>;

template <typename T>
constexpr bool is_safe_for_reinterpret_cast_v = is_safe_for_reinterpret_cast<T>::value;
} // namespace ringnet::traits