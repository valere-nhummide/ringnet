#pragma once

#include <cassert>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "requestQueue.hpp"
#include "socket.hpp"

class EchoServer {
    public:
	EchoServer(size_t max_clients_count = 10);
	~EchoServer();

	void listen(std::string_view listening_address, uint16_t listening_port);
	void stop();

    private:
	using FileDescriptor = int;
	void onCompletedAccept(FileDescriptor client_socket_fd);
	void onCompletedRead(const ReadRequest &completed_request);
	void onCompletedWrite(const WriteRequest &completed_request);
	void eventLoop();

	RequestQueue requests;

	using Socket = ServerSocket<TCP>;
	std::unique_ptr<Socket> listening_socket{};

	AcceptRequest accept_request{};
	std::unordered_map<FileDescriptor, ReadRequest> active_read_requests{};
	std::unordered_map<FileDescriptor, WriteRequest> active_write_requests{};

	using Buffer = std::array<std::byte, 1024>;
	std::unordered_map<FileDescriptor, Buffer> reception_buffers{};

	size_t max_clients_count;
	std::jthread worker_thread{};
	std::atomic_bool should_continue = true;
};

EchoServer::EchoServer(size_t max_clients_count_)
	: requests(1 + 2 * max_clients_count_), max_clients_count(max_clients_count_)
{
}

EchoServer::~EchoServer()
{
	stop();
}

void EchoServer::stop()
{
	should_continue = false;
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

	accept_request.listening_socket_fd = listening_socket->raw();
	status = requests.add(accept_request);
	if (status == QUEUE_FULL)
		throw std::runtime_error("Error accepting: IO queue full");

	worker_thread = std::jthread([this]() { eventLoop(); });
}

void EchoServer::eventLoop()
{
	while (should_continue) {
		SubmitStatus submit_status = requests.submit(std::chrono::milliseconds(100));

		if (requests.shouldContinueSubmitting(submit_status))
			continue;
		else if (submit_status < 0) {
			std::cerr << "Error submitting: " << strerror(-submit_status) << std::endl;
			continue;
		}
		requests.forEachCompletion([this](Completion cqe) {
			const Operation op = getOperation(cqe);
			switch (op) {
			case Operation::ACCEPT: {
				int client_socket_fd = cqe->res;
				if (client_socket_fd < 0) {
					std::cerr << "Server: Error accepting: " << strerror(-client_socket_fd)
						  << std::endl;
					/// @todo : Check if the request should be renewed
					return;
				}
				onCompletedAccept(client_socket_fd);
			} break;
			case Operation::READ: {
				int read_status = cqe->res;
				if (read_status == -1) {
					std::cerr << "Server: Error reading: " << strerror(-read_status) << std::endl;
					break;
				}
				ReadRequest completed_request;
				memcpy(&completed_request, io_uring_cqe_get_data(cqe), sizeof(completed_request));

				onCompletedRead(completed_request);
			} break;
			case Operation::WRITE: {
				int write_status = cqe->res;
				if (write_status == -1) {
					std::cerr << "Server: Error writing: " << strerror(-write_status) << std::endl;
					break;
				}
				WriteRequest completed_request =
					*reinterpret_cast<WriteRequest *>(io_uring_cqe_get_data(cqe));
				onCompletedWrite(completed_request);
			} break;
			default:
				std::cerr << "Server: Invalid operation (" << static_cast<uint32_t>(op) << std::endl;
				break;
			}
		});
	}
}

void EchoServer::onCompletedAccept(FileDescriptor client_socket_fd)
{
	if (active_read_requests.find(client_socket_fd) != active_read_requests.cend()) {
		std::cout << "Already reading from client socket " << client_socket_fd << "." << std::endl;
		return;
	}

	const auto [buffer_iter, was_inserted] = reception_buffers.emplace(std::make_pair(client_socket_fd, Buffer{}));
	assert(was_inserted);

	ReadRequest new_request;
	new_request.fd = client_socket_fd;
	new_request.bytes_read = buffer_iter->second;

	const auto [request_iter, _] = active_read_requests.emplace(std::make_pair(client_socket_fd, new_request));
	AddRequestStatus status = requests.add(request_iter->second);

	if (status != AddRequestStatus::OK)
		throw std::runtime_error("Adding read request failed");
}

void EchoServer::onCompletedRead(const ReadRequest &completed_request)
{
	FileDescriptor client_socket_fd = completed_request.fd;

	if (active_write_requests.find(client_socket_fd) != active_write_requests.cend()) {
		std::cout << "Already writing to client socket " << client_socket_fd << "." << std::endl;
		return;
	}

	WriteRequest new_request;
	new_request.fd = completed_request.fd;
	new_request.bytes_written = { std::make_move_iterator(completed_request.bytes_read.begin()),
				      std::make_move_iterator(completed_request.bytes_read.end()) };

	const auto [request_iter, _] = active_write_requests.emplace(std::make_pair(client_socket_fd, new_request));
	AddRequestStatus status = requests.add(request_iter->second);

	if (status != AddRequestStatus::OK)
		throw std::runtime_error("Adding write request failed");
}

void EchoServer::onCompletedWrite(const WriteRequest &completed_request)
{
	const size_t erased_count = active_write_requests.erase(completed_request.fd);
	assert(erased_count);
}
