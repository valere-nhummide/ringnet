#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <span>

#include <liburing.h>

#include "elio/subscriber.hpp"
#include "elio/traits/movable.hpp"
#include "elio/traits/reinterpretable.hpp"
namespace elio::uring
{

enum class Operation : uint8_t { ACCEPT, CONNECT, READ, WRITE };
struct RequestHeader {
	explicit RequestHeader(Operation op_) : op(op_){};
	explicit RequestHeader() : op(Operation::ACCEPT){};
	Operation op;
	void *user_data = nullptr;
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