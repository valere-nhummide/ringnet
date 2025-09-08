#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <span>

#include <liburing.h>

#include "ringnet/traits/reinterpretable.hpp"

namespace ringnet::uring
{
inline constexpr uint32_t HEADER_MAGIC_VALUE = 0xA1B2C3D4;

enum class Operation : uint32_t {
	ACCEPT = 0xA1A1A1A1,
	CONNECT = 0xB2B2B2B2,
	READ = 0xC3C3C3C3,
	READ_MULTISHOT = 0xD4D4D4D4,
	WRITE = 0xE5E5E5E5
};

struct RequestHeader {
	uint32_t magic = HEADER_MAGIC_VALUE;
	Operation op;
	void *user_data = nullptr;

	explicit RequestHeader(Operation op_) : op(op_){};
	explicit RequestHeader() : op(Operation::ACCEPT){};
	inline bool valid() const
	{
		return magic == HEADER_MAGIC_VALUE;
	}
};
static_assert(sizeof(RequestHeader) == 16);
static_assert(ringnet::traits::is_safe_for_reinterpret_cast_v<RequestHeader>);

struct AcceptRequest {
	RequestHeader header{ Operation::ACCEPT };
	int listening_socket_fd = -1;
};
static_assert(ringnet::traits::is_safe_for_reinterpret_cast_v<AcceptRequest>);

struct ConnectRequest {
	RequestHeader header{ Operation::CONNECT };
	int socket_fd = -1;
	const sockaddr *addr = nullptr;
	socklen_t addrlen = 0;
};
static_assert(ringnet::traits::is_safe_for_reinterpret_cast_v<ConnectRequest>);

/// @brief Liburing allows to use "provided buffers", where the kernel picks a suitable buffer when the given receive
/// operation is ready to actually receive data. Oppositely to buffers specified upfront, when the receive request is
/// emitted. Provided buffers need to be registered to the kernel. Multi-shot read requests require provided buffers.
/// rather than upfront

/// @brief Single-shot read request, with a reception buffer specified upfront. Needs to be renewed once completed.
struct ReadRequest {
	RequestHeader header{ Operation::READ };
	int fd = -1;
	std::span<std::byte> reception_buffer{};
};
static_assert(ringnet::traits::is_safe_for_reinterpret_cast_v<ReadRequest>);

/// @brief Multi-shot read request. Induces the use of provided buffers.
struct MultiShotReadRequest {
	RequestHeader header{ Operation::READ_MULTISHOT };
	int fd = -1;
	uint16_t buffer_group_id = -1;
};
static_assert(ringnet::traits::is_safe_for_reinterpret_cast_v<MultiShotReadRequest>);

struct WriteRequest {
	RequestHeader header{ Operation::WRITE };
	int fd = -1;
	std::span<const std::byte> bytes_written{};
};
static_assert(ringnet::traits::is_safe_for_reinterpret_cast_v<WriteRequest>);

inline std::ostream &operator<<(std::ostream &stream, const AcceptRequest &request)
{
	return (stream << "accept request for listening socket " << request.listening_socket_fd);
}
inline std::ostream &operator<<(std::ostream &stream, const ConnectRequest &request)
{
	return (stream << "connect request for socket " << request.socket_fd);
}
inline std::ostream &operator<<(std::ostream &stream, const ReadRequest &request)
{
	return (stream << "single shot read request using buffer of size " << request.reception_buffer.size()
		       << " bytes for socket " << request.fd);
}
inline std::ostream &operator<<(std::ostream &stream, const MultiShotReadRequest &request)
{
	return (stream << "multi shot read request using buffer group ID " << request.buffer_group_id << " for socket "
		       << request.fd);
}
inline std::ostream &operator<<(std::ostream &stream, const WriteRequest &request)
{
	return (stream << "write request of " << request.bytes_written.size() << " bytes for socket " << request.fd);
}

} // namespace ringnet::uring