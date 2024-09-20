#pragma once

#include <functional>
#include <tuple>

#include "elio/operation.hpp"
#include "elio/uring/request.hpp"

namespace elio
{
class Subscriber {
    public:
	template <class EventData>
	using Callback = std::function<void(EventData &&)>;

	/// @todo Make this a variadic template
	std::tuple<Callback<uring::AcceptRequest::Data>, Callback<uring::ReadRequest::Data>,
		   Callback<uring::WriteRequest::Data>, Callback<uring::ConnectRequest::Data>>
		handlers;

	template <class EventData>
	auto handle(EventData &&data) noexcept
	{
		return handler<EventData>()(std::move(data));
	}

	template <class EventData>
	Callback<EventData> &handler() noexcept
	{
		return std::get<Callback<EventData>>(handlers);
	}

	template <typename EventData>
	void on(Callback<EventData> f)
	{
		handler<EventData>() = std::move(f);
	}
};
} // namespace elio