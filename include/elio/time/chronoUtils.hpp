#pragma once

#include <chrono>

namespace elio::time::chrono_utils
{
template <typename Rep, typename Period>
__kernel_timespec to_timespec(const std::chrono::duration<Rep, Period> &duration)
{
	using namespace std::chrono;
	seconds s = duration_cast<seconds>(duration);
	nanoseconds ns = duration_cast<nanoseconds>(duration) - duration_cast<nanoseconds>(s);
	return __kernel_timespec{ .tv_sec = s.count(), .tv_nsec = ns.count() };
}
} // namespace elio::time::chrono_utils