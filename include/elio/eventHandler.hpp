#pragma once

#include <functional>
#include <tuple>

#include "elio/traits/movable.hpp"

namespace elio
{
/// @brief Do not move an EventHandler: its address is associated to requests submitted to the kernel, in order to
/// invoke one of its handlers on request completion. Hence, the address must remain untouched between submission and
/// completion.
template <class... Events>
class EventHandler : public traits::NonMovable {
    public:
	EventHandler() = default;

	template <class Event>
	using Callback = std::function<void(Event &&)>;

	std::tuple<Callback<Events>...> handlers{};

	template <class Event>
	auto handle(Event &&data) noexcept
	{
		const auto &handler_ = handler<Event>();
		if (handler_)
			return handler_(std::move(data));
	}

	template <class Event>
	Callback<Event> &handler() noexcept
	{
		return std::get<Callback<Event>>(handlers);
	}

	template <typename Event>
	void on(Callback<Event> f)
	{
		handler<Event>() = std::move(f);
	}
};
} // namespace elio