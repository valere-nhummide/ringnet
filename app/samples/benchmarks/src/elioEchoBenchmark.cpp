#include <iostream>
#include <string_view>
#include <thread>

#include "commandLineInterface.h"
#include "elio_impl/elioEchoClient.hpp"
#include "elio_impl/elioEchoServer.hpp"

#include "elio/eventLoop.hpp"

int main(int argc, char *argv[])
{
	CommandLineInterface cli(argc, argv);

	const std::string_view address = cli.address();
	const uint16_t port = cli.port();
	const uint64_t bytes_count = cli.bytes_count();

	elio::EventLoop loop(1024);

	ElioEchoServer server{ loop };
	server.listen(address, port);

	ElioEchoClient client{ loop, bytes_count };
	client.connect(address, port);

	std::jthread worker_thread = std::jthread([&loop]() { loop.run(); });

	client.waitForCompletion();
	client.printResults();

	loop.stop();
}