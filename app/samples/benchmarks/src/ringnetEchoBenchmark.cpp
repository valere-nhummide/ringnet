#include <iostream>
#include <string_view>
#include <thread>

#include "echoCLI.h"
#include "ringnet_impl/ringnetEchoClient.hpp"
#include "ringnet_impl/ringnetEchoServer.hpp"

#include "ringnet/eventLoop.hpp"

int main(int argc, char *argv[])
{
	EchoCLI cli(argc, argv);

	const std::string_view address = cli.address();
	const uint16_t port = cli.port();
	const uint64_t bytes_count = cli.bytes_count();

	ringnet::EventLoop loop(1024);

	RingnetEchoServer server{ loop };
	server.listen(address, port);

	RingnetEchoClient client{ loop, bytes_count };
	client.connect(address, port);

	loop.run();
}