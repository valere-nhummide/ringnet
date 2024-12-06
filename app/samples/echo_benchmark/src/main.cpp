#include <iostream>
#include <string_view>
#include <thread>

#include "client.hpp"
#include "commandLineInterface.h"
#include "server.hpp"

#include "elio/eventLoop.hpp"

using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
	CommandLineInterface cli(argc, argv);

	const std::string_view address = cli.address();
	const uint16_t port = cli.port();
	const uint64_t bytes_count = cli.bytes_count();

	elio::EventLoop loop(1024);

	EchoServer server{ loop };
	server.listen(address, port);

	EchoClient client{ loop, bytes_count };
	client.connect(address, port);

	std::jthread worker_thread = std::jthread([&loop]() { loop.run(); });
	
	client.waitForCompletion();
	client.printResults();

	loop.stop();
}