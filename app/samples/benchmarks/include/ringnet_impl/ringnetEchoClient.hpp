#pragma once

#include <cassert>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

#include "ringnet/net/connection.hpp"
#include "ringnet/net/connector.hpp"

class RingnetEchoClient {
    public:
	RingnetEchoClient(ringnet::EventLoop &loop, size_t min_bytes_count);

	void connect(std::string_view server_address, uint16_t server_port);
	void send(std::string_view message);

	void waitForCompletion();
	void printResults();

    private:
	void onConnected();

	ringnet::net::Connector<ringnet::net::TCP> connector;

	std::optional<ringnet::net::Connection> connection{};

	std::vector<std::byte> packet{ 1024, std::byte{ 'A' } };
	size_t min_bytes_count;
	size_t received_bytes_count = 0;

	using clock_t = std::chrono::high_resolution_clock;
	typename clock_t::time_point start{};
	typename clock_t::time_point stop{};
};

RingnetEchoClient::RingnetEchoClient(ringnet::EventLoop &loop, size_t min_bytes_count_)
	: connector(loop), min_bytes_count(min_bytes_count_)
{
}

void RingnetEchoClient::connect(std::string_view server_address, uint16_t server_port)
{
	std::cout << "Client: Connecting to " << server_address << ":" << server_port << "..." << std::endl;

	connector.onError([](ringnet::events::ErrorEvent &&event) {
		std::cerr << "Client: Error: " << event.what() << std::endl;
		std::terminate();
	});

	connector.onConnection([this](ringnet::net::Connection &&accepted_connection) {
		std::cout << "Client: Connected to " << accepted_connection.endpoint().fd << std::endl;
		connection = std::move(accepted_connection);
		onConnected();
	});

	MessagedStatus request_status = connector.asyncConnect(server_address, server_port);

	if (!request_status) {
		std::cerr << "Client: Could not connect to " << server_address << ":" << server_port << std::endl;
		std::cerr << request_status.what() << std::endl;
		std::terminate();
	}
}

void RingnetEchoClient::onConnected()
{
	assert(connection);

	connection->onError([](ringnet::events::ErrorEvent &&event) {
		std::cerr << "Client: Error: " << event.what() << std::endl;
		std::terminate();
	});

	connection->onRead([this](ringnet::events::ReadEvent &&event) {
		assert(event.bytes_read.size_bytes() == packet.size());

		received_bytes_count += event.bytes_read.size_bytes();
		if (received_bytes_count < min_bytes_count)
			connection->asyncWrite(packet);
		else {
			printResults();
			std::exit(0);
		}
	});
	connection->asyncRead();
	start = clock_t::now();
	connection->asyncWrite(packet);
}

void RingnetEchoClient::printResults()
{
	using namespace std::chrono;
	stop = clock_t::now();
	const nanoseconds elapsed = stop - start;

	double byte_rate = static_cast<double>(received_bytes_count) * 2.0; // Rx + Tx
	byte_rate /= 1'000'000.; // Byte -> MB
	byte_rate *= nanoseconds::period::den; // s -> ns
	byte_rate /= duration_cast<nanoseconds>(elapsed).count();

	std::cout << "Exchanged " << received_bytes_count << " bytes in ";
	if (elapsed.count() > seconds::period::den)
		std::cout << duration_cast<seconds>(elapsed).count() << "s";
	else if (elapsed.count() > milliseconds::period::den)
		std::cout << duration_cast<milliseconds>(elapsed).count() << "ms";
	else if (elapsed.count() > microseconds::period::den)
		std::cout << duration_cast<microseconds>(elapsed).count() << "μs";
	else
		std::cout << elapsed.count() << "ns";

	std::cout << " (" << byte_rate << " MB/s)\n";
}
