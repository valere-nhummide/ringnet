#pragma once

#include <functional>
#include <tuple>

#include "string.h"

/// @brief Those (except the error event) are specific to socket. Both the subscriber and the event loop would be
/// agnostic of those types, and defined it in a network or stream namespace. The file descriptor could be replaced by a
/// reference to a more complex socket object, which could hide io_uring requests to asynchronous
/// read/write/accept/connect methods.
namespace elio::events
{
struct ErrorEvent {
	int error_code{};
	inline auto what() const
	{
		return strerror(error_code);
	}
};

struct AcceptEvent {
	int client_fd{};
};

struct ConnectEvent {};

struct ReadEvent {
	int fd{};
	std::span<std::byte> bytes_read{};
};

struct WriteEvent {
	int fd{};
	std::span<const std::byte> bytes_written{};
};
} // namespace elio::events