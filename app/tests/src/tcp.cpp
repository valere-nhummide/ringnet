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

	net::Acceptor<net::TCP> server(loop);
	std::unique_ptr<net::Connection> server_connection;

	server.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
	server.onNewConnection([&server_connection](net::Connection &&new_connection) {
		server_connection = std::make_unique<net::Connection>(std::move(new_connection));
		server_connection->onError([](events::ErrorEvent &&event) { FAIL(event.what()); });
		server_connection->asyncRead();
	});

	server.listen("127.0.0.1", 4242);

	SUBCASE("Nominal case: client connects and sends data to the server")
	{
		net::Connector<net::TCP> client(loop);
		std::unique_ptr<net::Connection> client_connection;
		std::span<const std::byte> data = std::as_bytes(std::span("Hello, world!"));

		client.onError([](events::ErrorEvent &&event) { FAIL(event.what()); });

		client.onConnection([&](net::Connection &&connection) {
			server_connection->onRead([&loop, &data](events::ReadEvent &&event) {
				CHECK((event.bytes_read == data));
				loop.stop();
			});
			client_connection = std::make_unique<net::Connection>(std::move(connection));

			client_connection->asyncWrite(data);
		});

		client.asyncConnect("127.0.0.1", 4242);
		loop.run();
	}
}
