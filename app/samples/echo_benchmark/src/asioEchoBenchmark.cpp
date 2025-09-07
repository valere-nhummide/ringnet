#include <iostream>
#include <string_view>
#include <thread>

#include "asio_impl/asioEchoClient.hpp"
#include "asio_impl/asioEchoServer.hpp"
#include "commandLineInterface.h"

#include <asio.hpp>

int main(int argc, char *argv[])
{
	CommandLineInterface cli(argc, argv);

	const std::string_view address = cli.address();
	const uint16_t port = cli.port();
	const uint64_t bytes_count = cli.bytes_count();

	asio::io_context io_context;

	AsioEchoServer server{ io_context };
	server.listen(address, port);

	AsioEchoClient client{ io_context, bytes_count };
	client.connect(address, port);

	std::jthread worker_thread = std::jthread([&io_context]() { io_context.run(); });

	client.waitForCompletion();
	client.printResults();

	io_context.stop();
}
