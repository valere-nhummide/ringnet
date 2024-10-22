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
#include "elio/uring/request.hpp"

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

	template <class Func>
	void onError(Func &&callback);

	MessagedStatus asyncConnect(std::string_view server_address, uint16_t server_port);

	void waitForConnection();
	std::optional<Connection> getConnection();
	bool isConnected() const;

    private:
	elio::EventLoop &loop;
	elio::uring::ConnectRequest connect_request{};
	elio::Subscriber subscriber{};

	std::atomic<Connector<DP>::Status> connection_status = Connector<DP>::Status::DISCONNECTED;
	std::mutex connection_mutex{};
	std::condition_variable connection_cv{};

	using Socket = elio::net::Socket;
	Socket socket{};

	void onConnection();
};

template <DatagramProtocol DP>
Connector<DP>::Connector(elio::EventLoop &loop_) : loop(loop_)
{
	if constexpr (DP != TCP)
		return;

	subscriber.on<elio::events::ConnectEvent>([this](elio::events::ConnectEvent &&) { onConnection(); });
}

template <DatagramProtocol DP>
template <class Func>
void Connector<DP>::onError(Func &&callback)
{
	subscriber.on<elio::events::ErrorEvent> = std::move(callback);
}

template <DatagramProtocol DP>
MessagedStatus Connector<DP>::asyncConnect(std::string_view server_address, uint16_t server_port)
{
	if (connection_status == Status::PENDING)
		return MessagedStatus{ false, "Already pending connection" };

	const auto resolved_address = elio::net::resolve(server_address, server_port, DP, true);
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

	if constexpr (DP != TCP) {
		onConnection();
		return MessagedStatus{ true, "Connected" };
	}

	connect_request.socket_fd = socket.fd;
	std::tie(connect_request.addr, connect_request.addrlen) = resolved_address->as_sockaddr();

	auto status = loop.add(connect_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	connection_status = Status::PENDING;
	return MessagedStatus{ true, "Pending connection" };
}

template <DatagramProtocol DP>
std::optional<Connection> Connector<DP>::getConnection()
{
	if (connection_status != Status::CONNECTED)
		return std::nullopt;
	return Connection(loop, std::move(socket));
}

template <DatagramProtocol DP>
bool Connector<DP>::isConnected() const
{
	return (connection_status == Status::CONNECTED);
}

template <DatagramProtocol DP>
void Connector<DP>::waitForConnection()
{
	std::unique_lock<std::mutex> lock{ connection_mutex };
	while (connection_status != Status::CONNECTED)
		connection_cv.wait(lock, [this]() { return (connection_status == Status::CONNECTED); });
}

template <DatagramProtocol DP>
void Connector<DP>::onConnection()
{
	std::lock_guard<std::mutex> lock{ connection_mutex };
	connection_status = Status::CONNECTED;
	connection_cv.notify_all();
}
} // namespace elio::net