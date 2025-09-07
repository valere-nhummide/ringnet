#pragma once

#include <asio.hpp>
#include <iostream>
#include <memory>
#include <string_view>

class AsioEchoServer {
    public:
	AsioEchoServer(asio::io_context &io_context, size_t max_clients_count = 10);

	void listen(std::string_view listening_address, uint16_t listening_port);

    private:
	void start_accept();
	void handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket, const asio::error_code &error);
	void handle_read(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<std::vector<char>> buffer,
			 const asio::error_code &error, size_t bytes_transferred);
	void handle_write(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<std::vector<char>> buffer,
			  const asio::error_code &error, size_t bytes_transferred);

	asio::io_context &io_context_;
	asio::ip::tcp::acceptor acceptor_;
	size_t max_clients_count_{};
	std::vector<std::shared_ptr<asio::ip::tcp::socket>> connections_{};
};
