#pragma once

#include <array>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "elio/eventLoop.hpp"
#include "elio/net/tcp/clientSocket.hpp"
#include "elio/uring/request.hpp"

class EchoClient {
    public:
	EchoClient(elio::EventLoop &loop);
	~EchoClient();

	void connect(std::string_view server_address, uint16_t server_port);
	inline bool isConnected() const;
	void waitForConnection();

	enum SendStatus { OK, DISCONNECTED = -1 };
	SendStatus send(std::string &&message);

	void stop();

    private:
	void registerCallbacks();
	void onError(elio::events::ErrorEvent event);
	void onCompletedConnect();
	void onCompletedRead(elio::net::FileDescriptor fd, std::span<std::byte> bytes_read);
	void onCompletedWrite(elio::net::FileDescriptor fd, std::span<std::byte> &bytes_written);
	void addPendingWriteRequests();

	elio::EventLoop &loop;
	elio::Subscriber subscriber{};
	std::array<std::byte, 2048> reception_buffer{};

	elio::uring::ConnectRequest connect_request{};
	std::vector<elio::uring::WriteRequest> write_requests{};
	std::mutex write_requests_mutex{};

	using Socket = elio::net::tcp::ClientSocket;
	std::unique_ptr<Socket> socket{};
	bool is_connected = false;
	std::mutex connection_mutex{};
	std::condition_variable connection_cv{};
};

EchoClient::EchoClient(elio::EventLoop &loop_) : loop(loop_)
{
	registerCallbacks();
}

EchoClient::~EchoClient()
{
	stop();
}

EchoClient::SendStatus EchoClient::send(std::string &&message)
{
	if (!isConnected())
		return DISCONNECTED;

	elio::uring::WriteRequest request;
	request.fd = socket->raw();
	std::span<std::byte> as_bytes{ reinterpret_cast<std::byte *>(message.data()), message.size() };
	request.bytes_written = { std::make_move_iterator(as_bytes.begin()), std::make_move_iterator(as_bytes.end()) };
	{
		std::lock_guard lock(write_requests_mutex);
		write_requests.push_back(request);
	}
	return OK;
}

void EchoClient::stop()
{
	loop.stop();
}

void EchoClient::connect(std::string_view server_address, uint16_t server_port)
{
	std::cout << "Client: Connecting to " << server_address << ":" << server_port << "..." << std::endl;
	socket = std::make_unique<Socket>(loop);
	const auto resolve_status = socket->resolve(server_address, server_port);
	if (!resolve_status)
		std::cerr << "Error resolving address " + std::string(server_address) + ":" +
				     std::to_string(server_port) + ": " + resolve_status.what()
			  << "\n";

	socket->setOption(SO_REUSEADDR);

	connect_request.socket_fd = socket->raw();
	std::tie(connect_request.addr, connect_request.addrlen) = socket->getSockAddr();
	auto status = loop.add(connect_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		throw std::runtime_error("Error connecting: IO queue full");
}

inline bool EchoClient::isConnected() const
{
	return is_connected;
}

inline void EchoClient::waitForConnection()
{
	std::unique_lock<std::mutex> lock{ connection_mutex };
	while (!isConnected())
		connection_cv.wait(lock, [this]() { return isConnected(); });
}

void EchoClient::registerCallbacks()
{
	subscriber.on<elio::events::ErrorEvent>([this](elio::events::ErrorEvent &&event) { onError(event); });

	subscriber.on<elio::events::ConnectEvent>([this](elio::events::ConnectEvent &&) { onCompletedConnect(); });

	subscriber.on<elio::events::ReadEvent>(
		[this](elio::events::ReadEvent &&event) { onCompletedRead(event.fd, std::move(event.bytes_read)); });

	subscriber.on<elio::events::WriteEvent>(
		[this](elio::events::WriteEvent &&event) { onCompletedWrite(event.fd, event.bytes_written); });
}

void EchoClient::onError(elio::events::ErrorEvent event)
{
	std::cerr << "Error: " << event.what() << std::endl;
}

void EchoClient::onCompletedConnect()
{
	socket->read(reception_buffer, subscriber);

	{
		std::lock_guard<std::mutex> lock{ connection_mutex };
		is_connected = true;
		connection_cv.notify_all();
	}

	std::cout << "Client: Connected to server (socket " << socket->raw() << ")\n";
}

void EchoClient::onCompletedRead(elio::net::FileDescriptor, std::span<std::byte> bytes_read)
{
	std::cout << "Client: Received \"" << reinterpret_cast<char *>(bytes_read.data()) << "\"" << std::endl;
}

void EchoClient::onCompletedWrite(elio::net::FileDescriptor, std::span<std::byte> &bytes_written)
{
	std::cout << "Client: Sent \"" << reinterpret_cast<const char *>(bytes_written.data()) << "\"" << std::endl;
	write_requests.clear();
}

void EchoClient::addPendingWriteRequests()
{
	{
		std::lock_guard lock(write_requests_mutex);
		if (write_requests.empty())
			return;

		for (elio::uring::WriteRequest &request : write_requests)
			loop.add(request, subscriber);
	}
}
