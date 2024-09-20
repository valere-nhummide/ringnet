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
	EchoServer(size_t max_clients_count = 10);
	~EchoServer();

	void listen(std::string_view listening_address, uint16_t listening_port);
	void stop();

    private:
	using FileDescriptor = int;
	void onCompletedAccept(FileDescriptor client_socket_fd);
	void onCompletedRead(const elio::uring::ReadRequest &completed_request);
	void onCompletedWrite(const elio::uring::WriteRequest &completed_request);

	elio::EventLoop loop;
	elio::Subscriber subscriber{};

	using Socket = elio::net::ServerSocket<elio::net::TCP>;
	std::unique_ptr<Socket> listening_socket{};

	elio::uring::AcceptRequest accept_request{};
	std::unordered_map<FileDescriptor, elio::uring::ReadRequest> active_read_requests{};
	std::unordered_map<FileDescriptor, elio::uring::WriteRequest> active_write_requests{};

	using Buffer = std::array<std::byte, 1024>;
	std::unordered_map<FileDescriptor, Buffer> reception_buffers{};

	size_t max_clients_count;
	std::jthread worker_thread{};
};

EchoServer::EchoServer(size_t max_clients_count_)
	: loop(1 + 2 * max_clients_count_), max_clients_count(max_clients_count_)
{
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
	status = loop.requests.add(accept_request);
	if (status == elio::uring::QUEUE_FULL)
		throw std::runtime_error("Error accepting: IO queue full");

	worker_thread = std::jthread([this]() { loop.run(); });
}

void EchoServer::onCompletedAccept(FileDescriptor client_socket_fd)
{
	using namespace elio::uring;
	if (active_read_requests.find(client_socket_fd) != active_read_requests.cend()) {
		std::cout << "Already reading from client socket " << client_socket_fd << "." << std::endl;
		return;
	}

	const auto [buffer_iter, was_inserted] = reception_buffers.emplace(std::make_pair(client_socket_fd, Buffer{}));
	assert(was_inserted);

	ReadRequest new_request;
	new_request.data.fd = client_socket_fd;
	new_request.data.bytes_read = buffer_iter->second;

	const auto [request_iter, _] = active_read_requests.emplace(std::make_pair(client_socket_fd, new_request));
	AddRequestStatus status = loop.requests.add(request_iter->second);

	if (status != AddRequestStatus::OK)
		throw std::runtime_error("Adding read request failed");
}

void EchoServer::onCompletedRead(const elio::uring::ReadRequest &completed_request)
{
	using namespace elio::uring;
	FileDescriptor client_socket_fd = completed_request.data.fd;

	if (active_write_requests.find(client_socket_fd) != active_write_requests.cend()) {
		std::cout << "Already writing to client socket " << client_socket_fd << "." << std::endl;
		return;
	}

	WriteRequest new_request;
	new_request.data.fd = completed_request.data.fd;
	new_request.data.bytes_written = { std::make_move_iterator(completed_request.data.bytes_read.begin()),
					   std::make_move_iterator(completed_request.data.bytes_read.end()) };

	const auto [request_iter, _] = active_write_requests.emplace(std::make_pair(client_socket_fd, new_request));
	AddRequestStatus status = loop.requests.add(request_iter->second);

	if (status != AddRequestStatus::OK)
		throw std::runtime_error("Adding write request failed");
}

void EchoServer::onCompletedWrite(const elio::uring::WriteRequest &completed_request)
{
	const size_t erased_count = active_write_requests.erase(completed_request.data.fd);
	assert(erased_count);
}
