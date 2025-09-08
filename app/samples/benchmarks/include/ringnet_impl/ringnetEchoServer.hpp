#pragma once

#include <cassert>
#include <iostream>
#include <map>
#include <string_view>

#include "ringnet/net/acceptor.hpp"
#include "ringnet/net/connection.hpp"
#include "ringnet/net/endpoint.hpp"

class RingnetEchoServer {
    public:
	RingnetEchoServer(ringnet::EventLoop &loop_, size_t max_clients_count = 10);

	void listen(std::string_view listening_address, uint16_t listening_port);

    private:
	void registerCallbacks();

	ringnet::EventLoop &loop;
	ringnet::net::Acceptor<ringnet::net::TCP> acceptor;
	std::map<ringnet::net::Endpoint, ringnet::net::Connection> connections{};
	std::vector<std::byte> send_buffer{};
};

RingnetEchoServer::RingnetEchoServer(ringnet::EventLoop &loop_, size_t max_clients_count)
	: loop(loop_), acceptor(loop, max_clients_count)
{
	registerCallbacks();
}

void RingnetEchoServer::listen(std::string_view listening_address, uint16_t listening_port)
{
	auto listening = acceptor.listen(listening_address, listening_port);
	if (!listening)
		std::cout << "Server: Could not listen to " << listening_address << ":" << listening_port << std::endl;
}

void RingnetEchoServer::registerCallbacks()
{
	acceptor.onError(
		[](ringnet::events::ErrorEvent &&event) { std::cerr << "Error: " << event.what() << std::endl; });

	acceptor.onNewConnection([this](ringnet::net::Connection &&new_connection) {
		using namespace ringnet::uring;
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

		ringnet::net::Connection &connection = connection_iter->second;
		connection.onError([](ringnet::events::ErrorEvent &&event) {
			std::cerr << "Error: " << event.what() << std::endl;
			std::terminate();
		});

		connection.onRead([this, &connection](ringnet::events::ReadEvent &&event) {
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
