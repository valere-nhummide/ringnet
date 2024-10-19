#include <iostream>
#include <string_view>
#include <thread>

#include "client.hpp"
#include "server.hpp"

// tmp
#include "elio/eventLoop.hpp"

using namespace std::chrono_literals;

int main(int, char *[])
{
	elio::EventLoop loop(1024);
	std::jthread worker_thread = std::jthread([&loop]() { loop.run(); });

	EchoServer server{ loop };
	server.listen("127.0.0.1", 6789);
	std::this_thread::sleep_for(1s);

	EchoClient client{ loop };
	client.connect("127.0.0.1", 6789);
	client.send("Hello from client!");

	std::this_thread::sleep_for(1s);
	loop.stop();
}