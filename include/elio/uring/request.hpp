#pragma once

#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <span>

#include <liburing.h>

namespace elio::uring
{

enum class Operation : uint8_t { ACCEPT, CONNECT, READ, WRITE };
struct RequestHeader {
	explicit RequestHeader(Operation op_) : op(op_) {};
	explicit RequestHeader() : op(Operation::ACCEPT) {};
	Operation op;
	void *user_data = nullptr;
};

struct AcceptRequest {
	RequestHeader header{ Operation::ACCEPT };
	struct Data {
		int listening_socket_fd = -1;
	} data{};
};

struct ConnectRequest {
	RequestHeader header{ Operation::CONNECT };
	struct Data {
		int socket_fd = -1;
		const sockaddr *addr = nullptr;
		socklen_t addrlen = 0;
	} data{};
};

struct ReadRequest {
	RequestHeader header{ Operation::READ };
	struct Data {
		int fd = -1;
		std::span<std::byte> bytes_read{};
	} data{};
};
struct WriteRequest {
	RequestHeader header{ Operation::WRITE };
	struct Data {
		int fd = -1;
		std::vector<std::byte> bytes_written{};
	} data{};
};
} // namespace elio::uring