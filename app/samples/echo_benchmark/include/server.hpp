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

	elio::EventLoop &loop;
	elio::net::Acceptor<elio::net::TCP> acceptor;
	std::map<elio::net::Endpoint, elio::net::Connection> connections{};
	std::vector<std::byte> send_buffer{};
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
			std::cerr << "Server: Client already connected (endpoint  " << new_connection.endpoint().fd
				  << ")." << std::endl;
			return;
		}

		std::cout << "Server: Received client connection request (endpoint " << new_connection.endpoint().fd
			  << ")." << std::endl;

		const auto [connection_iter, was_inserted] =
			connections.try_emplace(new_connection.endpoint(), std::move(new_connection));
		assert(was_inserted);

		elio::net::Connection &connection = connection_iter->second;
		connection.onError([](elio::events::ErrorEvent &&event) {
			std::cerr << "Error: " << event.what() << std::endl;
			std::terminate();
		});

		connection.onRead([this, &connection](elio::events::ReadEvent &&event) {
			send_buffer.resize(event.bytes_read.size());
			std::ranges::copy(event.bytes_read, send_buffer.begin());
			auto status = connection.asyncWrite(send_buffer);
			if (!status)
				std::cerr << "Server: Error sending to client endpoint " << connection.endpoint().fd
					  << ": " << status.what() << std::endl;
		});

		auto status = connection.asyncRead();
		if (!status)
			std::cerr << "Server: Error reading from client endpoint " << connection.endpoint().fd << ": "
				  << status.what() << std::endl;
	});
}
