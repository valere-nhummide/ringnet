#include <cstdint>
#include <span>

namespace ringnet::test
{
template <typename T, size_t Extent1, size_t Extent2>
constexpr bool operator==(const std::span<T, Extent1> &lhs, const std::span<T, Extent2> &rhs)
{
	if (lhs.size() != rhs.size()) {
		return false;
	}
	return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}
} // namespace ringnet::test
