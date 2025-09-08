#pragma once

#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

class AsioEchoClient {
    public:
	AsioEchoClient(asio::io_context &io_context, size_t min_bytes_count);

	void connect(std::string_view server_address, uint16_t server_port);
	void printResults();

    private:
	void handle_connect(const asio::error_code &error);
	void handle_read(const asio::error_code &error, size_t bytes_transferred);
	void handle_write(const asio::error_code &error, size_t bytes_transferred);
	void send_packet();

	asio::io_context &io_context_;
	asio::ip::tcp::socket socket_;
	asio::ip::tcp::resolver resolver_;

	std::vector<char> packet_;
	size_t min_bytes_count_;
	size_t received_bytes_count_ = 0;

	using clock_t = std::chrono::high_resolution_clock;
	typename clock_t::time_point start_{};
	typename clock_t::time_point stop_{};
};
