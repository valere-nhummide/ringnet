#include "elio_impl/elioTcpReaderServer.hpp"

ElioTcpReader::ElioTcpReader(elio::EventLoop &loop_, size_t target_bytes_count)
	: loop(loop_), acceptor(loop, 10), target_bytes_count_(target_bytes_count)
{
	registerCallbacks();
}

void ElioTcpReader::listen(std::string_view listening_address, uint16_t listening_port)
{
	auto listening = acceptor.listen(listening_address, listening_port);
	if (!listening) {
		std::cerr << "TcpReader: Could not listen to " << listening_address << ":" << listening_port
			  << std::endl;
	} else {
		std::cout << "TcpReader: Listening on " << listening_address << ":" << listening_port << std::endl;
	}
}

void ElioTcpReader::registerCallbacks()
{
	acceptor.onError([](elio::events::ErrorEvent &&event) {
		std::cerr << "TcpReader: Acceptor error: " << event.what() << std::endl;
	});

	acceptor.onNewConnection([this](elio::net::Connection &&new_connection) {
		if (connections.find(new_connection.endpoint()) != connections.cend()) {
			std::cerr << "TcpReader: Client already connected (endpoint " << new_connection.endpoint().fd
				  << ")." << std::endl;
			return;
		}

		std::cout << "TcpReader: Received client connection request (endpoint " << new_connection.endpoint().fd
			  << ")." << std::endl;

		const auto [connection_iter, was_inserted] =
			connections.try_emplace(new_connection.endpoint(), std::move(new_connection));
		assert(was_inserted);

		elio::net::Connection &connection = connection_iter->second;
		connection.onError([this, &connection](elio::events::ErrorEvent &&event) {
			std::cerr << "TcpReader: Connection error: " << event.what() << std::endl;
			// Remove the connection from our map
			connections.erase(connection.endpoint());
		});

		connection.onRead([this](elio::events::ReadEvent &&event) { onRead(std::move(event)); });

		auto status = connection.asyncRead();
		if (!status) {
			std::cerr << "TcpReader: Error reading from client endpoint " << connection.endpoint().fd
				  << ": " << status.what() << std::endl;
		}
	});
}

void ElioTcpReader::onRead(elio::events::ReadEvent &&event)
{
	// Add the received bytes to our count
	received_bytes_count_ += event.bytes_read.size();

	if (received_bytes_count_ >= target_bytes_count_) {
		printResults();
		std::exit(0);
	}
}

void ElioTcpReader::printResults()
{
	std::cout << "TcpReader: Received " << received_bytes_count_ << " bytes" << std::endl;
}