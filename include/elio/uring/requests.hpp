#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <span>

#include <liburing.h>

#include "elio/traits/movable.hpp"
#include "elio/traits/reinterpretable.hpp"

namespace elio::uring
{

enum class Operation : uint32_t { ACCEPT = 0xA1A1A1A1, CONNECT = 0xB2B2B2B2, READ = 0xC3C3C3C3, WRITE = 0xD4D4D4D4 };

struct alignas(4) RequestHeader {
	static constexpr uint32_t MAGIC_VALUE = 0xA1B2C3D4;
	uint32_t magic = MAGIC_VALUE;
	Operation op;
	void *user_data = nullptr;

	explicit RequestHeader(Operation op_) : op(op_){};
	explicit RequestHeader() : op(Operation::ACCEPT){};
	inline bool valid() const
	{
		return magic == MAGIC_VALUE;
	}
};
static_assert(sizeof(RequestHeader) == 16);
static_assert(elio::traits::is_safe_for_reinterpret_cast_v<RequestHeader>);

struct AcceptRequest : public elio::traits::NonMovable {
	RequestHeader header{ Operation::ACCEPT };
	int listening_socket_fd = -1;
};
static_assert(elio::traits::is_safe_for_reinterpret_cast_v<AcceptRequest>);

struct ConnectRequest : public elio::traits::NonMovable {
	RequestHeader header{ Operation::CONNECT };
	int socket_fd = -1;
	const sockaddr *addr = nullptr;
	socklen_t addrlen = 0;
};
static_assert(elio::traits::is_safe_for_reinterpret_cast_v<ConnectRequest>);

struct ReadRequest : public elio::traits::NonMovable {
	RequestHeader header{ Operation::READ };
	int fd = -1;
	std::span<std::byte> bytes_read{};
};
static_assert(elio::traits::is_safe_for_reinterpret_cast_v<ReadRequest>);

struct WriteRequest : public elio::traits::NonMovable {
	RequestHeader header{ Operation::WRITE };
	int fd = -1;
	std::span<std::byte> bytes_written{};
};
static_assert(elio::traits::is_safe_for_reinterpret_cast_v<WriteRequest>);

std::ostream &operator<<(std::ostream &stream, const AcceptRequest &request)
{
	return (stream << "accept request for listening socket " << request.listening_socket_fd);
}
std::ostream &operator<<(std::ostream &stream, const ConnectRequest &request)
{
	return (stream << "connect request for socket " << request.socket_fd);
}
std::ostream &operator<<(std::ostream &stream, const ReadRequest &request)
{
	return (stream << "read request of " << request.bytes_read.size() << " bytes for socket " << request.fd);
}
std::ostream &operator<<(std::ostream &stream, const WriteRequest &request)
{
	return (stream << "write request of " << request.bytes_written.size() << " bytes for socket " << request.fd);
}

} // namespace elio::uring