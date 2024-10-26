#pragma once

#include <cassert>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

#include "elio/net/connection.hpp"
#include "elio/net/connector.hpp"

class EchoClient {
    public:
	EchoClient(elio::EventLoop &loop);

	void connect(std::string_view server_address, uint16_t server_port);
	void send(std::string_view message);

    private:
	void registerCallbacks();
	void onError(elio::events::ErrorEvent event);
	void onCompletedRead(std::span<std::byte> bytes_read);
	void onCompletedWrite(std::span<std::byte> bytes_written);

	elio::EventLoop &loop;
	std::optional<elio::net::Connection> connection{};
};

EchoClient::EchoClient(elio::EventLoop &loop_) : loop(loop_)
{
}

void EchoClient::send(std::string_view message)
{
	if (!connection)
		throw std::runtime_error("Client: Cannot send when disconnected");

	std::byte *message_view = reinterpret_cast<std::byte *>(const_cast<char *>(message.data()));
	std::span<std::byte> as_bytes{ message_view, message.size() };
	connection->asyncWrite(as_bytes);
}

void EchoClient::connect(std::string_view server_address, uint16_t server_port)
{
	std::cout << "Client: Connecting to " << server_address << ":" << server_port << "..." << std::endl;

	elio::net::Connector<elio::net::TCP> connector(loop);
	connector.onError([this](elio::events::ErrorEvent &&event) { onError(event); });
	connector.onConnection([this](elio::net::Connection &&accepted_connection) {
		std::cout << "Client: Connected to " << accepted_connection.endpoint().fd << std::endl;
		connection = std::move(accepted_connection);
		registerCallbacks();
	});

	MessagedStatus request_status = connector.asyncConnect(server_address, server_port);

	if (!request_status) {
		std::stringstream builder;
		builder << "Client: Could not connect to " << server_address << ":" << server_port << "..."
			<< std::endl;
		std::cerr << request_status.what() << std::endl;
		throw std::runtime_error(builder.str());
	}

	connector.waitForConnection();
}

void EchoClient::registerCallbacks()
{
	assert(connection);

	connection->onError([this](elio::events::ErrorEvent &&event) { onError(event); });

	connection->onRead([this](elio::events::ReadEvent &&event) { onCompletedRead(event.bytes_read); });

	connection->onWrite([this](elio::events::WriteEvent &&event) { onCompletedWrite(event.bytes_written); });
}

void EchoClient::onError(elio::events::ErrorEvent event)
{
	std::cerr << "Error: " << event.what() << std::endl;
}

void EchoClient::onCompletedRead(std::span<std::byte> bytes_read)
{
	std::cout << "Client: Received \"" << reinterpret_cast<char *>(bytes_read.data()) << "\"" << std::endl;
}

void EchoClient::onCompletedWrite(std::span<std::byte> bytes_written)
{
	std::cout << "Client: Sent \"" << reinterpret_cast<char *>(bytes_written.data()) << "\"" << std::endl;
}
