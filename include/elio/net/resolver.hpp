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

/// @brief Resolve the address, then if TCP, try to connect to a server. Create a Connection object once resolution (and
/// optional connection) succeed.
template <DatagramProtocol DP>
class Resolver {
    public:
	enum class Status {
		DISCONNECTED = -2,
		PENDING = -1,
		CONNECTED = 0,
	};

	explicit Resolver(elio::EventLoop &loop);

	template <class Func>
	void onError(Func &&callback);

	MessagedStatus ayncConnect(std::string_view server_address, uint16_t server_port);

	void waitForConnection();
	std::optional<Connection> getConnection();
	bool isConnected() const;

    private:
	elio::EventLoop &loop;
	elio::uring::ConnectRequest connect_request{};
	elio::Subscriber subscriber{};

	std::atomic<Resolver<DP>::Status> connection_status = Resolver<DP>::Status::DISCONNECTED;
	std::mutex connection_mutex{};
	std::condition_variable connection_cv{};

	using Socket = elio::net::Socket;
	Socket socket{};

	void onConnection();
};

template <DatagramProtocol DP>
Resolver<DP>::Resolver(elio::EventLoop &loop_) : loop(loop_)
{
	if constexpr (DP != TCP)
		return;

	subscriber.on<elio::events::ConnectEvent>([this](elio::events::ConnectEvent &&) { onConnection(); });
}

template <DatagramProtocol DP>
template <class Func>
void Resolver<DP>::onError(Func &&callback)
{
	subscriber.on<elio::events::ErrorEvent> = std::move(callback);
}

template <DatagramProtocol DP>
MessagedStatus Resolver<DP>::ayncConnect(std::string_view server_address, uint16_t server_port)
{
	if (connection_status == Status::PENDING)
		return MessagedStatus{ false, "Already pending connection" };

	const auto resolve_status = elio::net::resolve(socket, server_address, server_port, DP, true);
	if (!resolve_status)
		return MessagedStatus{ false, "Error resolving address " + std::string(server_address) + ":" +
						      std::to_string(server_port) + ": " + resolve_status.what() };

	auto socket_status = elio::net::set_option(socket, SO_REUSEADDR);
	if (!socket_status)
		return MessagedStatus{ false, "Error setting SO_REUSEADDR option to socket " +
						      std::string(server_address) + ":" + std::to_string(server_port) +
						      ": " + resolve_status.what() };

	if constexpr (DP != TCP) {
		onConnection();
		return MessagedStatus{ true, "Connected" };
	}

	assert(socket.address && "Socket should have an address once resolved");
	connect_request.socket_fd = socket.fd;
	std::tie(connect_request.addr, connect_request.addrlen) = elio::net::detail::as_sockaddr(*(socket.address));

	auto status = loop.add(connect_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	connection_status = Status::PENDING;
	return MessagedStatus{ true, "Pending connection" };
}

template <DatagramProtocol DP>
std::optional<Connection> Resolver<DP>::getConnection()
{
	if (connection_status != Status::CONNECTED)
		return std::nullopt;
	return Connection(loop, std::move(socket));
}

template <DatagramProtocol DP>
bool Resolver<DP>::isConnected() const
{
	return (connection_status == Status::CONNECTED);
}

template <DatagramProtocol DP>
void Resolver<DP>::waitForConnection()
{
	std::unique_lock<std::mutex> lock{ connection_mutex };
	while (connection_status != Status::CONNECTED)
		connection_cv.wait(lock, [this]() { return (connection_status == Status::CONNECTED); });
}

template <DatagramProtocol DP>
void Resolver<DP>::onConnection()
{
	std::lock_guard<std::mutex> lock{ connection_mutex };
	connection_status = Status::CONNECTED;
	connection_cv.notify_all();
}
} // namespace elio::net