#include <span>
#include <thread>

#include <doctest.h>

#include "elio/net/acceptor.hpp"
#include "elio/net/connection.hpp"
#include "elio/net/connector.hpp"

#include "operators.hpp"

using namespace elio;
using namespace elio::test;

TEST_CASE("TCP single server, single client")
{
	EventLoop loop(1024);

	auto server = loop.resource<net::Acceptor<net::TCP>>();
	std::unique_ptr<net::Connection> server_connection;

	server.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
	server.onNewConnection([&server_connection](net::Connection &&new_connection) {
		server_connection = std::make_unique<net::Connection>(std::move(new_connection));
		server_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
		server_connection->asyncRead();
	});

	server.listen("127.0.0.1", 4242);

	SUBCASE("Client connects and sends single message to the server")
	{
		auto client = loop.resource<net::Connector<net::TCP>>();
		std::unique_ptr<net::Connection> client_connection;
		std::span<const std::byte> data = std::as_bytes(std::span("Hello, world!"));

		client.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });

		client.onConnection([&](net::Connection &&connection) {
			server_connection->onRead([&loop, &data](events::ReadEvent &&event) {
				CHECK((event.bytes_read == data));
				loop.stop();
			});
			client_connection = std::make_unique<net::Connection>(std::move(connection));
			client_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
			client_connection->asyncWrite(data);
		});

		client.asyncConnect("127.0.0.1", 4242);
		loop.run();
	}

	SUBCASE("Multiple exchanges between client and server")
	{
		auto client = loop.resource<net::Connector<net::TCP>>();
		std::unique_ptr<net::Connection> client_connection;
		std::span<const std::byte> first_request = std::as_bytes(std::span("First request"));
		std::span<const std::byte> first_response = std::as_bytes(std::span("First response"));
		std::span<const std::byte> second_request = std::as_bytes(std::span("Second request"));
		std::span<const std::byte> second_response = std::as_bytes(std::span("Second response"));

		client.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });

		client.onConnection([&](net::Connection &&connection) {
			server_connection->onRead([&](events::ReadEvent &&event) {
				if (event.bytes_read == first_request)
					server_connection->asyncWrite(first_response);
				else if (event.bytes_read == second_request)
					server_connection->asyncWrite(second_response);
				else {
					std::string data(reinterpret_cast<const char *>(event.bytes_read.data()),
							 event.bytes_read.size());
					FAIL("Unexpected message content: ", data);
				}
			});

			client_connection = std::make_unique<net::Connection>(std::move(connection));
			client_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
			client_connection->onRead([&](events::ReadEvent &&event) {
				if (event.bytes_read == first_response)
					client_connection->asyncWrite(second_request);
				else if (event.bytes_read == second_response)
					loop.stop();
				else
					FAIL("Unexpected message content : ", event.bytes_read);
			});
			client_connection->asyncRead();
			client_connection->asyncWrite(first_request);
		});

		client.asyncConnect("127.0.0.1", 4242);
		loop.run();
	}
}
