#include <span>
#include <thread>

#include <doctest.h>

#include "elio/net/acceptor.hpp"
#include "elio/net/connection.hpp"
#include "elio/net/connector.hpp"

#include "operators.hpp"

using namespace elio;
using namespace elio::test;

inline std::string to_string(std::span<const std::byte> data)
{
	return std::string(reinterpret_cast<const char *>(data.data()), data.size());
}

using const_bytes_t = std::span<const std::byte>;
inline const_bytes_t to_bytes(std::string_view data)
{
	return std::as_bytes(std::span(data));
}

TEST_CASE("TCP single server, single client")
{
	EventLoop loop(1024);

	net::Acceptor<net::TCP> server = loop.resource<net::Acceptor<net::TCP>>();
	std::unique_ptr<net::Connection> server_connection;

	server.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
	server.onNewConnection([&server_connection](net::Connection &&new_connection) {
		std::cout << "Server: New connection from socket " << new_connection.endpoint().fd << std::endl;
		server_connection = std::make_unique<net::Connection>(std::move(new_connection));
		server_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
		server_connection->asyncRead();
		std::cout << "Server: Started reading from socket " << new_connection.endpoint().fd << std::endl;
	});

	server.listen("127.0.0.1", 4242);
	std::cout << "Server: Listening to 127.0.0.1:4242" << std::endl;

	SUBCASE("Client connects and sends single message to the server")
	{
		auto client = loop.resource<net::Connector<net::TCP>>();
		std::unique_ptr<net::Connection> client_connection;
		std::span<const std::byte> data = std::as_bytes(std::span("Hello, world!"));

		client.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });

		client.onConnection([&](net::Connection &&connection) {
			std::cout << "Client: Connected to server (socket " << connection.endpoint().fd << ")"
				  << std::endl;
			server_connection->onRead([&loop, &data](events::ReadEvent &&event) {
				std::cout << "Server: Received data from client (socket " << event.fd << ")"
					  << std::endl;
				CHECK((event.bytes_read == data));
				std::cout << "Stopping loop" << std::endl;
				loop.stop();
			});
			client_connection = std::make_unique<net::Connection>(std::move(connection));
			client_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
			std::cout << "Client: Sending data to server." << std::endl;
			client_connection->asyncWrite(data);
		});

		client.asyncConnect("127.0.0.1", 4242);
		loop.run();
	}

	SUBCASE("Multiple exchanges between client and server")
	{
		auto client = loop.resource<net::Connector<net::TCP>>();
		std::unique_ptr<net::Connection> client_connection;
		const_bytes_t first_request = to_bytes("First request");
		const_bytes_t first_response = to_bytes("First response");
		const_bytes_t second_request = to_bytes("Second request");
		const_bytes_t second_response = to_bytes("Second response");

		client.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });

		client.onConnection([&](net::Connection &&connection) {
			server_connection->onRead([&](events::ReadEvent &&event) {
				std::cout << "Server: Received \"" << to_string(event.bytes_read) << "\"" << std::endl;

				if (event.bytes_read == first_request)
					server_connection->asyncWrite(first_response);
				else if (event.bytes_read == second_request)
					server_connection->asyncWrite(second_response);
				else
					FAIL("Server: Unexpected received message: ", to_string(event.bytes_read));
			});

			client_connection = std::make_unique<net::Connection>(std::move(connection));
			client_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
			client_connection->onRead([&](events::ReadEvent &&event) {
				std::cout << "Client: Received \"" << to_string(event.bytes_read) << "\"" << std::endl;

				if (event.bytes_read == first_response)
					client_connection->asyncWrite(second_request);
				else if (event.bytes_read == second_response)
					loop.stop();
				else
					FAIL("Client: Unexpected received message: ", to_string(event.bytes_read));
			});
			client_connection->asyncRead();
			client_connection->asyncWrite(first_request);
		});

		client.asyncConnect("127.0.0.1", 4242);
		loop.run();
	}
}
