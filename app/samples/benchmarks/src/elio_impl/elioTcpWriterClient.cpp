#include "elio_impl/elioTcpWriterClient.hpp"

ElioTcpWriterClient::ElioTcpWriterClient(elio::EventLoop &loop, size_t chunk_size)
	: connector(loop), packet(chunk_size, std::byte{ 'A' })
{
}

void ElioTcpWriterClient::connect(std::string_view server_address, uint16_t server_port)
{
	std::cout << "TcpWriter: Connecting to " << server_address << ":" << server_port << "..." << std::endl;

	connector.onError([](elio::events::ErrorEvent &&event) {
		std::cerr << "TcpWriter: Error: " << event.what() << std::endl;
		std::terminate();
	});

	connector.onConnection([this](elio::net::Connection &&accepted_connection) {
		std::cout << "TcpWriter: Connected to " << accepted_connection.endpoint().fd << std::endl;
		connection = std::move(accepted_connection);
		onConnected();
	});

	MessagedStatus status = connector.asyncConnect(server_address, server_port);

	if (!status) {
		std::cerr << "TcpWriter: Could not connect to " << server_address << ":" << server_port << std::endl;
		std::cerr << status.what() << std::endl;
		std::terminate();
	}
}

void ElioTcpWriterClient::onConnected()
{
	assert(connection);

	connection->onError([](elio::events::ErrorEvent &&event) {
		std::cerr << "TcpWriter: Error: " << event.what() << std::endl;
		std::terminate();
	});

	connection->onWrite([this](elio::events::WriteEvent &&) {
		written_bytes += packet.size();
		remaining_bytes_before_print -= packet.size();
		if (remaining_bytes_before_print <= 0) {
			std::cout << "TcpWriter: Written " << written_bytes << " bytes" << std::endl;
			remaining_bytes_before_print = BYTES_PRINT_INTERVAL;
		}
		connection->asyncWrite(packet);
	});

	connection->asyncWrite(packet);
}
