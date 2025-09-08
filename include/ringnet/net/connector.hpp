#pragma once

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "ringnet/eventLoop.hpp"
#include "ringnet/net/sockets.hpp"
#include "ringnet/status.hpp"
#include "ringnet/uring/requests.hpp"

namespace ringnet::net
{

/// @brief Resolve a server hostname and port, then request a connection. Create a Connection object once resolution
/// connection succeeds.
template <DatagramProtocol DP>
class Connector {
    public:
	enum class Status {
		DISCONNECTED = -2,
		PENDING = -1,
		CONNECTED = 0,
	};

	explicit Connector(ringnet::EventLoop &loop);

	~Connector();

	template <class Func>
	void onError(Func &&callback);

	/// @brief Set the callback invoked on accepted connection.
	/// @tparam Func Type of the callback (a callable taking a Connection rvalue as argument)
	/// @param user_callback User callback
	/// @todo Use a concept for the callback.
	template <class Func>
	void onConnection(Func user_callback);

	MessagedStatus asyncConnect(std::string_view server_address, uint16_t server_port);

    private:
	ringnet::EventLoop &loop;
	std::unique_ptr<ringnet::Subscriber> subscriber = std::make_unique<ringnet::Subscriber>();

	ringnet::net::ResolvedAddress resolved_address{};

	std::atomic<Connector<DP>::Status> connection_status = Connector<DP>::Status::DISCONNECTED;

	using FileDescriptor = ringnet::net::FileDescriptor;
	FileDescriptor socket{};
};

template <DatagramProtocol DP>
Connector<DP>::Connector(ringnet::EventLoop &loop_) : loop(loop_)
{
}

template <DatagramProtocol DP>
Connector<DP>::~Connector()
{
	if (socket)
		loop.cancel(socket.fd);
}

template <DatagramProtocol DP>
template <class Func>
void Connector<DP>::onError(Func &&callback)
{
	subscriber->on<ringnet::events::ErrorEvent>(std::move(callback));
}

template <DatagramProtocol DP>
template <class Func>
void Connector<DP>::onConnection(Func user_callback)
{
	subscriber->on<ringnet::events::ConnectEvent>([this, user_callback](const ringnet::events::ConnectEvent &) {
		user_callback(Connection{ loop, std::move(socket) });
		connection_status = Status::CONNECTED;
	});
}

template <DatagramProtocol DP>
MessagedStatus Connector<DP>::asyncConnect(std::string_view server_address, uint16_t server_port)
{
	if (connection_status == Status::PENDING)
		return MessagedStatus{ false, "Already pending connection" };

	resolved_address = ringnet::net::resolve(server_address, server_port, DP, true);
	if (!resolved_address)
		return MessagedStatus{ false, "Error resolving address " + std::string(server_address) + ":" +
						      std::to_string(server_port) + ": " +
						      resolved_address.error().what() };

	assert(resolved_address->ip_version().has_value());
	socket = ::socket(resolved_address->ip_version().value(), DP, 0);

	auto socket_status = ringnet::net::set_option(socket, SO_REUSEADDR);
	if (!socket_status)
		return MessagedStatus{ false, "Error setting SO_REUSEADDR option to socket " +
						      std::string(server_address) + ":" + std::to_string(server_port) +
						      ": " + socket_status.what() };

	ringnet::uring::ConnectRequest request;
	request.socket_fd = socket.fd;
	std::tie(request.addr, request.addrlen) = resolved_address->as_sockaddr();

	auto status = loop.add(request, subscriber.get());
	if (status == ringnet::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	connection_status = Status::PENDING;
	return MessagedStatus{ true, "Pending connection" };
}

} // namespace ringnet::net