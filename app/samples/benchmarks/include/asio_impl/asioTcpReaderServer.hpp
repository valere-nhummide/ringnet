#pragma once

#include <asio.hpp>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

class AsioTcpReader {
    public:
	AsioTcpReader(asio::io_context &io_context, size_t target_bytes_count);

	void listen(std::string_view listening_address, uint16_t listening_port);
	void printResults();

    private:
	void start_accept();
	void handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket, const asio::error_code &error);
	void handle_read(std::shared_ptr<asio::ip::tcp::socket> socket, const asio::error_code &error,
			 size_t bytes_transferred);

	asio::io_context &io_context_;
	asio::ip::tcp::acceptor acceptor_;
	std::vector<std::byte> read_buffer_{ 4096, std::byte{ 0 } };
	size_t target_bytes_count_;
	size_t received_bytes_count_ = 0;
};
