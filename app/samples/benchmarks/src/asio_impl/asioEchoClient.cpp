#include "asio_impl/asioEchoClient.hpp"

AsioEchoClient::AsioEchoClient(asio::io_context &io_context, size_t min_bytes_count)
	: io_context_(io_context), socket_(io_context), resolver_(io_context), packet_(1024, 'A'),
	  min_bytes_count_(min_bytes_count)
{
}

void AsioEchoClient::connect(std::string_view server_address, uint16_t server_port)
{
	std::cout << "Client: Connecting to " << server_address << ":" << server_port << "..." << std::endl;

	auto endpoints = resolver_.resolve(server_address, std::to_string(server_port));

	asio::async_connect(socket_, endpoints, [this](const asio::error_code &error, const asio::ip::tcp::endpoint &) {
		handle_connect(error);
	});
}

void AsioEchoClient::handle_connect(const asio::error_code &error)
{
	if (!error) {
		std::cout << "Client: Connected successfully" << std::endl;
		start_ = clock_t::now();
		send_packet();
	} else {
		std::cerr << "Client: Connection failed: " << error.message() << std::endl;
	}
}

void AsioEchoClient::send_packet()
{
	asio::async_write(socket_, asio::buffer(packet_),
			  [this](const asio::error_code &error, size_t bytes_transferred) {
				  handle_write(error, bytes_transferred);
			  });
}

void AsioEchoClient::handle_write(const asio::error_code &error, size_t)
{
	if (!error) {
		// Start reading the echo response
		auto buffer = std::make_shared<std::vector<char>>(1024);
		socket_.async_read_some(asio::buffer(*buffer),
					[this, buffer](const asio::error_code &ec, size_t bytes_transferred_) {
						handle_read(ec, bytes_transferred_);
					});
	} else {
		std::cerr << "Client: Write error: " << error.message() << std::endl;
	}
}

void AsioEchoClient::handle_read(const asio::error_code &error, size_t bytes_transferred)
{
	if (!error) {
		received_bytes_count_ += bytes_transferred;

		if (received_bytes_count_ < min_bytes_count_) {
			// Continue the ping-pong
			send_packet();
		} else {
			// We've reached our target
			printResults();
			std::exit(0);
		}
	} else {
		std::cerr << "Client: Read error: " << error.message() << std::endl;
	}
}

void AsioEchoClient::printResults()
{
	using namespace std::chrono;
	stop_ = clock_t::now();
	const nanoseconds elapsed = stop_ - start_;

	double byte_rate = static_cast<double>(received_bytes_count_) * 2.0; // Rx + Tx
	byte_rate /= 1'000'000.; // Byte -> MB
	byte_rate *= nanoseconds::period::den; // s -> ns
	byte_rate /= duration_cast<nanoseconds>(elapsed).count();

	std::cout << "Exchanged " << received_bytes_count_ << " bytes in ";
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
