#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#include "elio/eventLoop.hpp"
#include "elio/net/sockets.hpp"
#include "elio/status.hpp"
#include "elio/uring/requests.hpp"

namespace elio::net
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

	explicit Connector(elio::EventLoop &loop);

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

	void waitForConnection();

    private:
	elio::EventLoop &loop;
	std::shared_ptr<elio::uring::ConnectRequest> connect_request = std::make_shared<elio::uring::ConnectRequest>();
	std::shared_ptr<elio::Subscriber> subscriber = std::make_shared<elio::Subscriber>();

	elio::net::ResolvedAddress resolved_address{};

	std::atomic<Connector<DP>::Status> connection_status = Connector<DP>::Status::DISCONNECTED;
	std::mutex connection_mutex{};
	std::condition_variable connection_cv{};

	using FileDescriptor = elio::net::FileDescriptor;
	FileDescriptor socket{};
};

template <DatagramProtocol DP>
Connector<DP>::Connector(elio::EventLoop &loop_) : loop(loop_)
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
	subscriber->on<elio::events::ErrorEvent>(std::move(callback));
}

template <DatagramProtocol DP>
template <class Func>
void Connector<DP>::onConnection(Func user_callback)
{
	subscriber->on<elio::events::ConnectEvent>([this, user_callback](const elio::events::ConnectEvent &) {
		user_callback(Connection{ loop, std::move(socket) });
		std::lock_guard<std::mutex> lock{ connection_mutex };
		connection_status = Status::CONNECTED;
		connection_cv.notify_all();
	});
}

template <DatagramProtocol DP>
MessagedStatus Connector<DP>::asyncConnect(std::string_view server_address, uint16_t server_port)
{
	if (connection_status == Status::PENDING)
		return MessagedStatus{ false, "Already pending connection" };

	resolved_address = elio::net::resolve(server_address, server_port, DP, true);
	if (!resolved_address)
		return MessagedStatus{ false, "Error resolving address " + std::string(server_address) + ":" +
						      std::to_string(server_port) + ": " +
						      resolved_address.error().what() };

	assert(resolved_address->ip_version().has_value());
	socket = ::socket(resolved_address->ip_version().value(), DP, 0);

	auto socket_status = elio::net::set_option(socket, SO_REUSEADDR);
	if (!socket_status)
		return MessagedStatus{ false, "Error setting SO_REUSEADDR option to socket " +
						      std::string(server_address) + ":" + std::to_string(server_port) +
						      ": " + socket_status.what() };

	connect_request->socket_fd = socket.fd;
	std::tie(connect_request->addr, connect_request->addrlen) = resolved_address->as_sockaddr();

	auto status = loop.add(connect_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	connection_status = Status::PENDING;
	return MessagedStatus{ true, "Pending connection" };
}

template <DatagramProtocol DP>
void Connector<DP>::waitForConnection()
{
	std::unique_lock<std::mutex> lock{ connection_mutex };
	while (connection_status != Status::CONNECTED)
		connection_cv.wait(lock, [this]() { return (connection_status == Status::CONNECTED); });
}

} // namespace elio::net