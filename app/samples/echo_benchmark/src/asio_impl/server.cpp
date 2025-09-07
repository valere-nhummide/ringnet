#include "asio_impl/server.hpp"

AsioEchoServer::AsioEchoServer(asio::io_context &io_context, size_t max_clients_count)
	: io_context_(io_context), acceptor_(io_context), max_clients_count_(max_clients_count)
{
}

void AsioEchoServer::listen(std::string_view listening_address, uint16_t listening_port)
{
	try {
		asio::ip::tcp::endpoint endpoint(asio::ip::make_address(listening_address.data()), listening_port);
		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen();

		std::cout << "Server: Listening on " << listening_address << ":" << listening_port << std::endl;
		start_accept();
	} catch (const std::exception &e) {
		std::cerr << "Server: Could not listen to " << listening_address << ":" << listening_port << " - "
			  << e.what() << std::endl;
	}
}

void AsioEchoServer::start_accept()
{
	auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
	acceptor_.async_accept(*socket,
			       [this, socket](const asio::error_code &error) { handle_accept(socket, error); });
}

void AsioEchoServer::handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket, const asio::error_code &error)
{
	if (!error) {
		std::cout << "Server: Received client connection request" << std::endl;

		// Store the connection
		connections_.push_back(socket);

		// Start reading from this client
		auto buffer = std::make_shared<std::vector<char>>(1024);
		socket->async_read_some(asio::buffer(*buffer),
					[this, socket, buffer](const asio::error_code &ec, size_t bytes_transferred) {
						handle_read(socket, buffer, ec, bytes_transferred);
					});
	} else {
		std::cerr << "Server: Accept error: " << error.message() << std::endl;
	}

	// Continue accepting new connections
	start_accept();
}

void AsioEchoServer::handle_read(std::shared_ptr<asio::ip::tcp::socket> socket,
				 std::shared_ptr<std::vector<char>> buffer, const asio::error_code &error,
				 size_t bytes_transferred)
{
	if (!error) {
		// Echo the data back
		auto send_buffer =
			std::make_shared<std::vector<char>>(buffer->begin(), buffer->begin() + bytes_transferred);
		asio::async_write(*socket, asio::buffer(*send_buffer),
				  [this, socket, send_buffer](const asio::error_code &ec, size_t bytes_transferred_) {
					  handle_write(socket, send_buffer, ec, bytes_transferred_);
				  });
	} else {
		std::cerr << "Server: Read error: " << error.message() << std::endl;
		// Remove the connection from our list
		connections_.erase(std::remove(connections_.begin(), connections_.end(), socket), connections_.end());
	}
}

void AsioEchoServer::handle_write(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<std::vector<char>>,
				  const asio::error_code &error, size_t)
{
	if (!error) {
		// Continue reading from this client
		auto read_buffer = std::make_shared<std::vector<char>>(1024);
		socket->async_read_some(asio::buffer(*read_buffer),
					[this, socket, read_buffer](const asio::error_code &ec,
								    size_t bytes_transferred) {
						handle_read(socket, read_buffer, ec, bytes_transferred);
					});
	} else {
		std::cerr << "Server: Write error: " << error.message() << std::endl;
		// Remove the connection from our list
		connections_.erase(std::remove(connections_.begin(), connections_.end(), socket), connections_.end());
	}
}
