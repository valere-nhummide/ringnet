#pragma once

#include <functional>
#include <mutex>
#include <tuple>

#include "ringnet/traits/movable.hpp"

namespace ringnet
{
/// @brief Do not move an EventHandler: its address is associated to requests submitted to the kernel, in order to
/// invoke one of its handlers on request completion. Hence, the address must remain untouched between submission and
/// completion.
template <class... Events>
class EventHandler : public traits::NonMovable {
	template <class Event>

	using Callback = std::function<void(Event &&)>;

    public:
	EventHandler() = default;

	template <class Event>
	auto handle(Event &&data) noexcept
	{
		std::lock_guard<std::mutex> lock{ mutex };
		const auto &handler_ = handler<Event>();
		if (handler_)
			return handler_(std::move(data));
	}

	template <typename Event>
	void on(Callback<Event> f)
	{
		std::lock_guard<std::mutex> lock{ mutex };
		handler<Event>() = std::move(f);
	}

    private:
	std::tuple<Callback<Events>...> handlers{};
	std::mutex mutex{};

	template <class Event>
	Callback<Event> &handler() noexcept
	{
		return std::get<Callback<Event>>(handlers);
	}
};
} // namespace ringnet