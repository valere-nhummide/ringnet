#include "asio_impl/asioTcpReaderServer.hpp"

AsioTcpReader::AsioTcpReader(asio::io_context &io_context, size_t target_bytes_count)
	: io_context_(io_context), acceptor_(io_context), target_bytes_count_(target_bytes_count)
{
}

void AsioTcpReader::listen(std::string_view listening_address, uint16_t listening_port)
{
	try {
		asio::ip::tcp::endpoint endpoint(asio::ip::make_address(listening_address.data()), listening_port);
		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen();

		std::cout << "TcpReader: Listening on " << listening_address << ":" << listening_port << std::endl;
		start_accept();
	} catch (const std::exception &e) {
		std::cerr << "TcpReader: Could not listen to " << listening_address << ":" << listening_port << " - "
			  << e.what() << std::endl;
	}
}

void AsioTcpReader::start_accept()
{
	auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
	acceptor_.async_accept(*socket,
			       [this, socket](const asio::error_code &error) { handle_accept(socket, error); });
}

void AsioTcpReader::handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket, const asio::error_code &error)
{
	if (!error) {
		std::cout << "TcpReader: Received client connection request" << std::endl;
		socket->async_read_some(asio::buffer(read_buffer_),
					[this, socket](const asio::error_code &ec, size_t size) {
						handle_read(socket, ec, size);
					});
	} else {
		std::cerr << "TcpReader: Accept error: " << error.message() << std::endl;
	}

	// Continue accepting new connections
	start_accept();
}

void AsioTcpReader::handle_read(std::shared_ptr<asio::ip::tcp::socket> socket, const asio::error_code &error,
				size_t received_bytes_count)
{
	if (!error) {
		received_bytes_count_ += received_bytes_count;

		if (received_bytes_count_ >= target_bytes_count_) {
			printResults();
			socket->close();
			std::exit(0);
		}

		socket->async_read_some(asio::buffer(read_buffer_),
					[this, socket](const asio::error_code &ec, size_t size) {
						handle_read(socket, ec, size);
					});
	} else {
		std::cerr << "TcpReader: Read error: " << error.message() << std::endl;
		// Remove the connection from our list
	}
}

void AsioTcpReader::printResults()
{
	using namespace std::chrono;
	std::cout << "TcpReader: Received " << received_bytes_count_ << " bytes" << std::endl;
}
