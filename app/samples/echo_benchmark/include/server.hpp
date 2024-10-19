#pragma once

#include <cassert>
#include <iostream>
#include <map>
#include <string_view>

#include "elio/net/acceptor.hpp"
#include "elio/net/connection.hpp"
#include "elio/net/endpoint.hpp"

class EchoServer {
    public:
	EchoServer(elio::EventLoop &loop_, size_t max_clients_count = 10);

	void listen(std::string_view listening_address, uint16_t listening_port);

    private:
	void registerCallbacks();
	void onRead(elio::events::ReadEvent &&event);

	elio::EventLoop &loop;
	elio::net::Acceptor<elio::net::TCP> acceptor;
	std::map<elio::net::Endpoint, elio::net::Connection> connections{};
};

EchoServer::EchoServer(elio::EventLoop &loop_, size_t max_clients_count)
	: loop(loop_), acceptor(loop, max_clients_count)
{
	registerCallbacks();
}

void EchoServer::listen(std::string_view listening_address, uint16_t listening_port)
{
	auto listening = acceptor.listen(listening_address, listening_port);
	if (!listening)
		std::cout << "Server: Could not listen to " << listening_address << ":" << listening_port << std::endl;
}

void EchoServer::registerCallbacks()
{
	acceptor.onError([](elio::events::ErrorEvent &&event) { std::cerr << "Error: " << event.what() << std::endl; });

	acceptor.onNewConnection([this](elio::net::Connection &&new_connection) {
		using namespace elio::uring;
		if (connections.find(new_connection.endpoint()) != connections.cend()) {
			std::cout << "Server: Client already connected (endpoint  " << new_connection.endpoint().fd
				  << ")." << std::endl;
			return;
		}

		std::cout << "Server: Received client connection request (endpoint " << new_connection.endpoint().fd
			  << ")." << std::endl;

		new_connection.onRead([this](elio::events::ReadEvent &&event) { onRead(std::move(event)); });

		auto status = new_connection.asyncRead();
		if (!status)
			std::cerr << "Server: Error reading from client endpoint " << new_connection.endpoint().fd
				  << ": " << status.what() << std::endl;

		const auto [connection_iter, was_inserted] =
			connections.try_emplace(new_connection.endpoint(), std::move(new_connection));
		assert(was_inserted);
	});
}

void EchoServer::onRead(elio::events::ReadEvent &&event)
{
	using namespace elio::uring;

	const char *message_data = reinterpret_cast<const char *>(event.bytes_read.data());
	const std::string_view as_characters{ message_data, message_data + event.bytes_read.size() };

	std::cout << "Server: Received message on socket " << event.fd << ": " << as_characters << std::endl;

	const auto connection_iter = connections.find(elio::net::Endpoint{ .fd = event.fd });
	if (connection_iter == connections.cend())
		std::cerr << "Server: No connection attached to socket " << event.fd << std::endl;

	std::vector<std::byte> response = { std::make_move_iterator(event.bytes_read.begin()),
					    std::make_move_iterator(event.bytes_read.end()) };

	const auto write_status = connection_iter->second.asyncWrite(std::move(response));

	if (!write_status)
		std::cerr << "Server: could not write to client (" << write_status.what() << std::endl;
}
