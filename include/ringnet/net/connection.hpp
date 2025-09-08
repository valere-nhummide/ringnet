#pragma once

#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "ringnet/eventLoop.hpp"
#include "ringnet/net/endpoint.hpp"
#include "ringnet/net/sockets.hpp"
#include "ringnet/status.hpp"
#include "ringnet/uring/bufferRing.hpp"
#include "ringnet/uring/requests.hpp"

namespace ringnet::net
{
/// @brief A connection holds everything required to ensure requests submitted to the io_uring are not dangling.
///  In TCP, a connection is created:
///  - when a client connects successfully to a server, using a resolver
///  - when a server accepts a connection on its listening socket, using an acceptor
/// In UDP, only address resolution is required to create a connection.
class Connection {
    public:
	Connection(ringnet::EventLoop &loop, net::FileDescriptor &&socket);

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
	std::reference_wrapper<ringnet::EventLoop> loop;

	using FileDescriptor = ringnet::net::FileDescriptor;
	FileDescriptor socket;

	Endpoint endpoint_;

	/// @brief The addresses of the objects submitted to the kernel should not change until they are completed:
	/// neither the subscriber, which holds the handles, nor the requests themselves, nor any associated buffer.
	/// Using smart pointers ensures that their address maintains valid when moving the Connection object around.
	std::unique_ptr<ringnet::Subscriber> subscriber = std::make_unique<ringnet::Subscriber>();
};

template <class Func>
void Connection::onError(Func &&callback)
{
	subscriber->on<ringnet::events::ErrorEvent>(std::move(callback));
}

template <class Func>
void Connection::onRead(Func &&callback)
{
	subscriber->on<ringnet::events::ReadEvent>(std::move(callback));
}

template <class Func>
void Connection::onWrite(Func &&callback)
{
	subscriber->on<ringnet::events::WriteEvent>(std::move(callback));
}
} // namespace ringnet::net