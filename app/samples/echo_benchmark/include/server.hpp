#pragma once

#include <cassert>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "elio/net/socket.hpp"
#include "elio/uring/request.hpp"
#include "elio/uring/requestQueue.hpp"

class EchoServer {
    public:
	EchoServer(elio::EventLoop &loop_, size_t max_clients_count = 10);
	~EchoServer();

	void listen(std::string_view listening_address, uint16_t listening_port);
	void stop();

    private:
	void registerCallbacks();
	void onCompletedAccept(elio::net::FileDescriptor client_socket_fd);
	void onCompletedRead(elio::net::FileDescriptor fd, std::span<std::byte> bytes_read);
	void onCompletedWrite(elio::net::FileDescriptor fd, std::span<std::byte> &bytes_written);

	elio::EventLoop &loop;
	elio::Subscriber subscriber{};

	using Socket = elio::net::ServerSocket<elio::net::TCP>;
	std::unique_ptr<Socket> listening_socket{};

	elio::uring::AcceptRequest accept_request{};
	std::unordered_map<elio::net::FileDescriptor, elio::uring::ReadRequest> active_read_requests{};
	std::unordered_map<elio::net::FileDescriptor, elio::uring::WriteRequest> active_write_requests{};

	using Buffer = std::array<std::byte, 1024>;
	std::unordered_map<elio::net::FileDescriptor, Buffer> reception_buffers{};

	size_t max_clients_count;
};

EchoServer::EchoServer(elio::EventLoop &loop_, size_t max_clients_count_)
	: loop(loop_), max_clients_count(max_clients_count_)
{
	registerCallbacks();
}

EchoServer::~EchoServer()
{
	stop();
}

void EchoServer::stop()
{
	loop.stop();
}

void EchoServer::listen(std::string_view listening_address, uint16_t listening_port)
{
	listening_socket = std::make_unique<Socket>(listening_address, listening_port);
	int status = listening_socket->bind();
	if (status)
		throw std::runtime_error("Error binding to " + std::string(listening_address) + ":" +
					 std::to_string(listening_port) + ": " + std::string(strerror(errno)));

	status = listening_socket->listen(max_clients_count);
	if (status)
		throw std::runtime_error("Error listening to " + std::string(listening_address) + ":" +
					 std::to_string(listening_port) + ": " + std::string(strerror(errno)));

	accept_request.data.listening_socket_fd = listening_socket->raw();
	status = loop.add(accept_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		throw std::runtime_error("Error accepting: IO queue full");

	std::cout << "Server: Listening to " << listening_address << ":" << listening_port << " (socket "
		  << listening_socket->raw() << ")..." << std::endl;
}

void EchoServer::registerCallbacks()
{
	subscriber.on<elio::events::AcceptEvent>(
		[this](elio::events::AcceptEvent &&event) { onCompletedAccept(event.client_fd); });

	subscriber.on<elio::events::ReadEvent>(
		[this](elio::events::ReadEvent &&event) { onCompletedRead(event.fd, std::move(event.bytes_read)); });

	subscriber.on<elio::events::WriteEvent>(
		[this](elio::events::WriteEvent &&event) { onCompletedWrite(event.fd, event.bytes_written); });
}

void EchoServer::onCompletedAccept(elio::net::FileDescriptor client_socket_fd)
{
	using namespace elio::uring;
	if (active_read_requests.find(client_socket_fd) != active_read_requests.cend()) {
		std::cout << "Already reading from client socket " << client_socket_fd << "." << std::endl;
		return;
	}

	std::cout << "Server: Received client connection request (socket " << client_socket_fd << ")." << std::endl;

	const auto [buffer_iter, was_inserted] = reception_buffers.emplace(std::make_pair(client_socket_fd, Buffer{}));
	assert(was_inserted);

	ReadRequest new_request;
	new_request.data.fd = client_socket_fd;
	new_request.data.bytes_read = buffer_iter->second;

	const auto [request_iter, _] = active_read_requests.emplace(std::make_pair(client_socket_fd, new_request));
	AddRequestStatus status = loop.add(request_iter->second, subscriber);

	if (status != AddRequestStatus::OK)
		throw std::runtime_error("Adding read request failed");
}

void EchoServer::onCompletedRead(elio::net::FileDescriptor fd, std::span<std::byte> bytes_read)
{
	using namespace elio::uring;
	elio::net::FileDescriptor client_socket_fd = fd;

	if (active_write_requests.find(client_socket_fd) != active_write_requests.cend()) {
		std::cout << "Already writing to client socket " << client_socket_fd << "." << std::endl;
		return;
	}

	WriteRequest new_request;
	new_request.data.fd = fd;
	new_request.data.bytes_written = { std::make_move_iterator(bytes_read.begin()),
					   std::make_move_iterator(bytes_read.end()) };

	const auto [request_iter, _] = active_write_requests.emplace(std::make_pair(client_socket_fd, new_request));
	AddRequestStatus status = loop.add(request_iter->second, subscriber);

	if (status != AddRequestStatus::OK)
		throw std::runtime_error("Adding write request failed");
}
void EchoServer::onCompletedWrite(elio::net::FileDescriptor fd, std::span<std::byte> &)
{
	const size_t erased_count = active_write_requests.erase(fd);
	assert(erased_count);
}
