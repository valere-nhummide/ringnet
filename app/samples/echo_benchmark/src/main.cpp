#include <iostream>
#include <string_view>
#include <thread>

#include "client.hpp"
#include "server.hpp"

using namespace std::chrono_literals;

int main(int, char *[])
{
	EchoServer server;
	server.listen("127.0.0.1", 6789);
	std::this_thread::sleep_for(1s);

	EchoClient client;
	client.connect("127.0.0.1", 6789);
	client.waitForConnection();
	client.send("Hello from client!");

	std::this_thread::sleep_for(1s);
}