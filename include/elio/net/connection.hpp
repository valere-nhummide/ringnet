#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#include "elio/eventLoop.hpp"
#include "elio/net/endpoint.hpp"
#include "elio/net/sockets.hpp"
#include "elio/status.hpp"
#include "elio/uring/request.hpp"

namespace elio::net
{
/// @brief A connection holds everything required to ensure requests submitted to the io_uring are not dangling.
///  In TCP, a connection is created:
///  - when a client connects successfully to a server, using a resolver
///  - when a server accepts a connection on its listening socket, using an acceptor
/// In UDP, only address resolution is required to create a connection.
class Connection {
    public:
	Connection(elio::EventLoop &loop, net::Socket &&socket);

	MessagedStatus asyncRead();
	MessagedStatus asyncWrite(std::span<std::byte> sent_bytes);

	/// @todo Add concepts
	template <class Func>
	void onError(Func &&callback);

	template <class Func>
	void onRead(Func &&callback);

	template <class Func>
	void onWrite(Func &&callback);

	const Endpoint &endpoint() const;

    private:
	std::reference_wrapper<elio::EventLoop> loop;

	using Socket = elio::net::Socket;
	Socket socket;

	Endpoint endpoint_;

	/// @brief The addresses of the objects submitted to the kernel should not change until they are completed:
	/// neither the subscriber, which holds the handles, nor the requests themselves, nor any associated buffer.
	/// Using smart pointers ensures that their address maintains valid when moving the Connection object around.
	std::unique_ptr<elio::Subscriber> subscriber = std::make_unique<elio::Subscriber>();
	std::unique_ptr<elio::uring::ReadRequest> read_request = std::make_unique<elio::uring::ReadRequest>();
	std::unique_ptr<elio::uring::WriteRequest> write_request = std::make_unique<elio::uring::WriteRequest>();

	using Buffer = std::array<std::byte, 2048>;
	std::unique_ptr<Buffer> reception_buffer = std::make_unique<Buffer>();
};

Connection::Connection(elio::EventLoop &loop_, Socket &&socket_)
	: loop(std::ref(loop_)), socket(std::move(socket_)), endpoint_{ .fd = socket.fd }
{
}

template <class Func>
void Connection::onError(Func &&callback)
{
	subscriber->on<elio::events::ErrorEvent>(std::move(callback));
}

template <class Func>
void Connection::onRead(Func &&callback)
{
	subscriber->on<elio::events::ReadEvent>(std::move(callback));
}

template <class Func>
void Connection::onWrite(Func &&callback)
{
	subscriber->on<elio::events::WriteEvent>(std::move(callback));
}

MessagedStatus Connection::asyncRead()
{
	read_request->fd = socket.fd;
	read_request->bytes_read = *reception_buffer;
	uring::AddRequestStatus status = loop.get().add(*read_request, *subscriber);
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

MessagedStatus Connection::asyncWrite(std::span<std::byte> sent_bytes)
{
	write_request->fd = socket.fd;
	write_request->bytes_written = sent_bytes;
	uring::AddRequestStatus status = loop.get().add(*write_request, *subscriber);
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

const Endpoint &Connection::endpoint() const
{
	return endpoint_;
}

} // namespace elio::net