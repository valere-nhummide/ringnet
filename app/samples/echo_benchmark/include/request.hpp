#pragma once

#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <span>

#include <liburing.h>

enum class Operation : uint8_t { ACCEPT, CONNECT, READ, WRITE };

inline Operation getOperation(const io_uring_cqe *cqe)
{
	Operation op;
	std::memcpy(&op, io_uring_cqe_get_data(cqe), sizeof(op));
	return op;
}

template <Operation OP>
struct Request {
    private:
	Operation op = OP;
};

struct AcceptRequest : Request<Operation::ACCEPT> {
	int listening_socket_fd = -1;
};

struct ConnectRequest : Request<Operation::CONNECT> {
	int socket_fd = -1;
	const sockaddr *addr = nullptr;
	socklen_t addrlen = 0;
};

struct ReadRequest : Request<Operation::READ> {
	int fd = -1;
	std::span<std::byte> bytes_read{};
};

struct WriteRequest : Request<Operation::WRITE> {
	int fd = -1;
	std::vector<std::byte> bytes_written{};
};
