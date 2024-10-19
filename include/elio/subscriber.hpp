#pragma once

#include <functional>
#include <tuple>

/// @todo Should be removed, and use variadic template
#include "elio/events.hpp"

namespace elio
{
class Subscriber {
    public:
	template <class Event>
	using Callback = std::function<void(Event &&)>;

	/// @todo Make this a variadic template
	std::tuple<Callback<events::ErrorEvent>, Callback<events::AcceptEvent>, Callback<events::ReadEvent>,
		   Callback<events::WriteEvent>, Callback<events::ConnectEvent>>
		handlers{};

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