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
#include "elio/uring/bufferRing.hpp"
#include "elio/uring/requests.hpp"

namespace elio::net
{
/// @brief A connection holds everything required to ensure requests submitted to the io_uring are not dangling.
///  In TCP, a connection is created:
///  - when a client connects successfully to a server, using a resolver
///  - when a server accepts a connection on its listening socket, using an acceptor
/// In UDP, only address resolution is required to create a connection.
class Connection {
    public:
	Connection(elio::EventLoop &loop, net::FileDescriptor &&socket);

	Connection(Connection &&) = default;
	Connection &operator=(Connection &&) = default;

	~Connection();

	MessagedStatus asyncRead();
	MessagedStatus asyncWrite(std::span<const std::byte> sent_bytes);

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

	using FileDescriptor = elio::net::FileDescriptor;
	FileDescriptor socket;

	Endpoint endpoint_;

	/// @brief The addresses of the objects submitted to the kernel should not change until they are completed:
	/// neither the subscriber, which holds the handles, nor the requests themselves, nor any associated buffer.
	/// Using smart pointers ensures that their address maintains valid when moving the Connection object around.
	std::unique_ptr<elio::Subscriber> subscriber = std::make_unique<elio::Subscriber>();
};

Connection::Connection(elio::EventLoop &loop_, FileDescriptor &&socket_)
	: loop(std::ref(loop_)), socket(std::move(socket_)), endpoint_{ .fd = socket.fd }
{
}

Connection::~Connection()
{
	if (socket)
		loop.get().cancel(socket.fd);
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
	elio::uring::MultiShotReadRequest request;
	request.fd = socket.fd;
	uring::AddRequestStatus status = loop.get().add(request, subscriber.get());
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

MessagedStatus Connection::asyncWrite(std::span<const std::byte> sent_bytes)
{
	elio::uring::WriteRequest request;
	request.fd = socket.fd;
	request.bytes_written = sent_bytes;
	uring::AddRequestStatus status = loop.get().add(request, subscriber.get());
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

const Endpoint &Connection::endpoint() const
{
	return endpoint_;
}

} // namespace elio::net