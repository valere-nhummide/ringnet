#pragma once

#include <atomic>
#include <limits>

#include "elio/eventLoop.hpp"
#include "elio/net/connection.hpp"
#include "elio/net/sockets.hpp"
#include "elio/status.hpp"
#include "elio/uring/requests.hpp"

namespace elio::net
{

/// @brief Bind, listen and accept incoming connection requests. Create a connection object, made available in the
/// acceptance callback.
template <DatagramProtocol DP>

class Acceptor {
    public:
	enum class Status { NOT_LISTENING = 0, LISTENING = 1 };

	explicit Acceptor(elio::EventLoop &loop, size_t max_connections = std::numeric_limits<size_t>::max());

	~Acceptor();

	/// @brief Set the callback invoked on new accepted connection.
	/// @tparam Func Type of the callback (a callable taking a Connection rvalue as argument)
	/// @param user_callback User callback
	/// @todo Use a concept for the callback.
	template <class Func>
	void onNewConnection(Func user_callback);

	template <class Func>
	void onError(Func &&user_callback);

	/// @brief Bind, listen and accept incoming connection requests. Create a connection object, made available in
	/// the acceptance callback.
	/// @param listening_address (Local) address to listen to.
	/// @param listening_port Port to listen to.
	/// @return An error if setting up the multi-shot accept request failed. Success otherwise.
	MessagedStatus listen(std::string_view listening_address, uint16_t listening_port);

    private:
	elio::EventLoop &loop;
	std::shared_ptr<elio::uring::AcceptRequest> accept_request = std::make_shared<elio::uring::AcceptRequest>();
	std::shared_ptr<elio::Subscriber> subscriber = std::make_shared<elio::Subscriber>();

	std::atomic<Status> status = Status::NOT_LISTENING;
	size_t max_connections;

	using Socket = elio::net::Socket;
	Socket listening_socket{};
};

template <DatagramProtocol DP>
Acceptor<DP>::Acceptor(EventLoop &loop_, size_t max_connections_) : loop(loop_), max_connections(max_connections_)
{
}

template <DatagramProtocol DP>
Acceptor<DP>::~Acceptor()
{
	if (listening_socket)
		loop.cancel(listening_socket.fd);
}

template <DatagramProtocol DP>
template <class Func>
void Acceptor<DP>::onError(Func &&user_callback)
{
	subscriber->on<elio::events::ErrorEvent>(std::move(user_callback));
}

template <DatagramProtocol DP>
template <class Func>
void Acceptor<DP>::onNewConnection(Func user_callback)
{
	subscriber->on<elio::events::AcceptEvent>([this, user_callback](const elio::events::AcceptEvent &event) {
		user_callback(Connection{ loop, Socket{ event.client_fd } });
	});
}

template <DatagramProtocol DP>
MessagedStatus Acceptor<DP>::listen(std::string_view listening_address, uint16_t listening_port)
{
	if (status == Status::LISTENING)
		return MessagedStatus{ false, "Already listening" };

	const auto resolved_address = elio::net::resolve(listening_address, listening_port, DP, true);
	if (!resolved_address)
		return MessagedStatus{ false, "Error resolving address " + std::string(listening_address) + ":" +
						      std::to_string(listening_port) + ": " +
						      resolved_address.error().what() };

	assert(resolved_address->ip_version().has_value());
	listening_socket = ::socket(resolved_address->ip_version().value(), DP, 0);

	auto socket_status = elio::net::set_option(listening_socket, SO_REUSEADDR);
	if (!socket_status)
		return MessagedStatus{ false, "Error setting SO_REUSEADDR option to socket " +
						      std::string(listening_address) + ":" +
						      std::to_string(listening_port) + ": " + socket_status.what() };

	socket_status = elio::net::bind(listening_socket, *resolved_address);
	if (!socket_status)
		return MessagedStatus{ false, "Error binding to " + std::string(listening_address) + ":" +
						      std::to_string(listening_port) + ": " + socket_status.what() };

	socket_status = elio::net::listen(listening_socket, max_connections);
	if (!socket_status)
		return MessagedStatus{ false, "Error listening to " + std::string(listening_address) + ":" +
						      std::to_string(listening_port) + ": " + socket_status.what() };

	accept_request->listening_socket_fd = listening_socket.fd;
	auto uring_status = loop.add(accept_request, subscriber);
	if (uring_status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	status = Status::LISTENING;

	return MessagedStatus{ true, "Pending connection requests" };
}
} // namespace elio::net