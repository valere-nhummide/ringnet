#pragma once

#include <atomic>
#include <cassert>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

#include "elio/net/connection.hpp"
#include "elio/net/connector.hpp"

class ElioEchoClient {
    public:
	ElioEchoClient(elio::EventLoop &loop, size_t min_bytes_count);

	void connect(std::string_view server_address, uint16_t server_port);
	void send(std::string_view message);

	void waitForCompletion();
	void printResults();

    private:
	void onConnected();

	elio::net::Connector<elio::net::TCP> connector;

	std::optional<elio::net::Connection> connection{};

	std::vector<std::byte> packet{ 1024, std::byte{ 'A' } };
	size_t min_bytes_count;
	size_t received_bytes_count = 0;

	using clock_t = std::chrono::high_resolution_clock;
	typename clock_t::time_point start{};
	typename clock_t::time_point stop{};

	std::mutex completion_mutex{};
	std::condition_variable completion_cv{};
	std::atomic_bool has_completed = false;
};

ElioEchoClient::ElioEchoClient(elio::EventLoop &loop, size_t min_bytes_count_)
	: connector(loop), min_bytes_count(min_bytes_count_)
{
}

void ElioEchoClient::connect(std::string_view server_address, uint16_t server_port)
{
	std::cout << "Client: Connecting to " << server_address << ":" << server_port << "..." << std::endl;

	connector.onError([](elio::events::ErrorEvent &&event) {
		std::cerr << "Client: Error: " << event.what() << std::endl;
		std::terminate();
	});

	connector.onConnection([this](elio::net::Connection &&accepted_connection) {
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

void ElioEchoClient::onConnected()
{
	assert(connection);

	connection->onError([](elio::events::ErrorEvent &&event) {
		std::cerr << "Client: Error: " << event.what() << std::endl;
		std::terminate();
	});

	connection->onRead([this](elio::events::ReadEvent &&event) {
		assert(event.bytes_read.size_bytes() == packet.size());

		received_bytes_count += event.bytes_read.size_bytes();
		if (received_bytes_count < min_bytes_count)
			connection->asyncWrite(packet);
		else {
			std::lock_guard<std::mutex> lock{ completion_mutex };
			has_completed = true;
			completion_cv.notify_all();
		}
	});
	connection->asyncRead();
	start = clock_t::now();
	connection->asyncWrite(packet);
}

void ElioEchoClient::printResults()
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
		std::cout << duration_cast<seconds>(elapsed);
	else if (elapsed.count() > milliseconds::period::den)
		std::cout << duration_cast<milliseconds>(elapsed);
	else if (elapsed.count() > microseconds::period::den)
		std::cout << duration_cast<microseconds>(elapsed);
	else
		std::cout << elapsed;

	std::cout << " (" << byte_rate << " MB/s)\n";
}

void ElioEchoClient::waitForCompletion()
{
	std::unique_lock<std::mutex> lock{ completion_mutex };
	while (!has_completed)
		completion_cv.wait(lock, [this]() { return has_completed.load(); });
}
