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
#include "elio/net/socket.hpp"
class EchoClient {
    public:
	EchoClient();
	~EchoClient();

	void connect(std::string_view server_address, uint16_t server_port);
	inline bool isConnected() const;
	void waitForConnection();

	enum SendStatus { OK, DISCONNECTED = -1 };
	SendStatus send(std::string &&message);

	void stop();

    private:
	void registerCallbacks();
	void onCompletedConnect(elio::net::FileDescriptor fd);
	void onCompletedRead(elio::net::FileDescriptor fd, std::span<std::byte> bytes_read);
	void onCompletedWrite(elio::net::FileDescriptor fd, std::vector<std::byte> &bytes_written);
	void addPendingWriteRequests();

	elio::EventLoop loop{ 3 };
	elio::Subscriber subscriber{};
	std::jthread worker_thread{};
	std::array<std::byte, 2048> reception_buffer{};

	elio::uring::ConnectRequest connect_request{};
	elio::uring::ReadRequest read_request{};
	std::vector<elio::uring::WriteRequest> write_requests{};
	std::mutex write_requests_mutex{};

	using Socket = elio::net::ClientSocket<elio::net::TCP>;
	std::unique_ptr<Socket> socket{};
	bool is_connected = false;
	std::mutex connection_mutex{};
	std::condition_variable connection_cv{};
};

EchoClient::EchoClient()
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
	request.data.fd = socket->raw();
	std::span<std::byte> as_bytes{ reinterpret_cast<std::byte *>(message.data()), message.size() };
	request.data.bytes_written = { std::make_move_iterator(as_bytes.begin()),
				       std::make_move_iterator(as_bytes.end()) };
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
	socket = std::make_unique<Socket>(server_address, server_port);

	connect_request.data.socket_fd = socket->raw();
	std::tie(connect_request.data.addr, connect_request.data.addrlen) = socket->getSockAddr();
	auto status = loop.add(connect_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		throw std::runtime_error("Error connecting: IO queue full");

	worker_thread = std::jthread([this]() { loop.run(); });
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
	subscriber.on<elio::uring::ConnectRequest::Data>(
		[this](elio::uring::ConnectRequest::Data &&data) { onCompletedConnect(data.socket_fd); });

	subscriber.on<elio::uring::ReadRequest::Data>([this](elio::uring::ReadRequest::Data &&data) {
		onCompletedRead(data.fd, std::move(data.bytes_read));
	});

	subscriber.on<elio::uring::WriteRequest::Data>(
		[this](elio::uring::WriteRequest::Data &&data) { onCompletedWrite(data.fd, data.bytes_written); });
}

void EchoClient::onCompletedConnect(elio::net::FileDescriptor fd)
{
	if (fd != socket->raw())
		throw std::runtime_error("Error connecting: Unexpected file descriptor");

	read_request.data.fd = socket->raw();
	read_request.data.bytes_read = reception_buffer;
	elio::uring::AddRequestStatus status = loop.add(read_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		throw std::runtime_error("Error reading: IO queue full");

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

void EchoClient::onCompletedWrite(elio::net::FileDescriptor, std::vector<std::byte> &bytes_written)
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
