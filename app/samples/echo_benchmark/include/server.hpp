#pragma once

#include <cassert>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "elio/net/tcp/clientSocket.hpp"
#include "elio/net/tcp/serverSocket.hpp"
#include "elio/uring/request.hpp"
#include "elio/uring/requestQueue.hpp"

class EchoServer {
    public:
	EchoServer(elio::EventLoop &loop_, size_t max_clients_count = 10);
	~EchoServer();

	int listen(std::string_view listening_address, uint16_t listening_port);
	void stop();

    private:
	void registerCallbacks();
	void onError(elio::events::ErrorEvent event);
	void onCompletedAccept(elio::net::FileDescriptor client_socket_fd);
	void onCompletedRead(elio::net::FileDescriptor fd, std::span<std::byte> bytes_read);
	void onCompletedWrite(elio::net::FileDescriptor fd, std::span<std::byte> &bytes_written);

	elio::EventLoop &loop;
	elio::Subscriber subscriber{};

	using Socket = elio::net::tcp::ServerSocket;
	std::unique_ptr<Socket> listening_socket{};

	elio::uring::AcceptRequest accept_request{};
	std::unordered_map<elio::net::FileDescriptor, elio::net::tcp::ClientSocket> client_sockets{};
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

int EchoServer::listen(std::string_view listening_address, uint16_t listening_port)
{
	listening_socket = std::make_unique<Socket>(loop);
	auto resolve_status = listening_socket->resolve(listening_address, listening_port);
	if (!resolve_status) {
		std::cerr << "Error resolving " + std::string(listening_address) + ":" +
				     std::to_string(listening_port) + ": " + resolve_status.what()
			  << "\n";
		return resolve_status.code();
	}
	auto bind_status = listening_socket->bind();
	if (!bind_status) {
		std::cerr << "Error binding to " + std::string(listening_address) + ":" +
				     std::to_string(listening_port) + ": " + resolve_status.what()
			  << "\n";
		return resolve_status.code();
	}
	auto listen_status = listening_socket->listen(max_clients_count);
	if (!listen_status) {
		std::cerr << "Error listening to " + std::string(listening_address) + ":" +
				     std::to_string(listening_port) + ": " + resolve_status.what()
			  << "\n";
		return resolve_status.code();
	}
	accept_request.listening_socket_fd = listening_socket->raw();
	auto status = loop.add(accept_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		throw std::runtime_error("Error accepting: IO queue full");

	std::cout << "Server: Listening to " << listening_address << ":" << listening_port << " (socket "
		  << listening_socket->raw() << ")..." << std::endl;

	return 0;
}

void EchoServer::registerCallbacks()
{
	subscriber.on<elio::events::ErrorEvent>([this](elio::events::ErrorEvent &&event) { onError(event); });

	subscriber.on<elio::events::AcceptEvent>(
		[this](elio::events::AcceptEvent &&event) { onCompletedAccept(event.client_fd); });

	subscriber.on<elio::events::ReadEvent>(
		[this](elio::events::ReadEvent &&event) { onCompletedRead(event.fd, std::move(event.bytes_read)); });

	subscriber.on<elio::events::WriteEvent>(
		[this](elio::events::WriteEvent &&event) { onCompletedWrite(event.fd, event.bytes_written); });
}

void EchoServer::onError(elio::events::ErrorEvent event)
{
	std::cerr << "Error: " << event.what() << std::endl;
}

void EchoServer::onCompletedAccept(elio::net::FileDescriptor client_socket_fd)
{
	using namespace elio::uring;
	if (client_sockets.find(client_socket_fd) != client_sockets.cend()) {
		std::cout << "Client already connected on socket " << client_socket_fd << "." << std::endl;
		return;
	}

	std::cout << "Server: Received client connection request (socket " << client_socket_fd << ")." << std::endl;

	const auto [buffer_iter, buffer_was_inserted] =
		reception_buffers.emplace(std::make_pair(client_socket_fd, Buffer{}));
	assert(buffer_was_inserted);

	const auto [client_socket_iter, socket_was_inserted] =
		client_sockets.try_emplace(client_socket_fd, loop, client_socket_fd);
	assert(socket_was_inserted);

	auto status = client_socket_iter->second.read(buffer_iter->second, subscriber);
	if (!status)
		std::cerr << "Server: Error reading from client socket: " << status.what() << std::endl;
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
	new_request.fd = fd;
	new_request.bytes_written = { std::make_move_iterator(bytes_read.begin()),
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
