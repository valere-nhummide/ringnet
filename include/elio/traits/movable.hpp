#pragma once

namespace elio::traits
{
/// @brief The addresses of the objects submitted to the kernel should not change until they are completed.
struct NonMovable {
	NonMovable() = default;
	NonMovable(NonMovable &&) = delete;
	NonMovable &operator=(NonMovable &&) = delete;
};
} // namespace elio::traits