#pragma once

#include <array>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "request.hpp"
#include "requestQueue.hpp"
#include "socket.hpp"

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
	void eventLoop();
	void onCompletedConnect();
	void onCompletedRead(const ReadRequest &completed_request);
	void onCompletedWrite(const WriteRequest &completed_request);
	void addPendingWriteRequests();

	RequestQueue requests;
	std::array<std::byte, 2048> reception_buffer{};
	std::jthread worker_thread{};
	std::atomic_bool should_continue = true;

	ConnectRequest connect_request{};
	ReadRequest read_request{};
	std::vector<WriteRequest> write_requests{};
	std::mutex write_requests_mutex{};

	using Socket = ClientSocket<TCP>;
	std::unique_ptr<Socket> socket{};
	bool is_connected = false;
	std::mutex connection_mutex{};
	std::condition_variable connection_cv{};
};

EchoClient::EchoClient() : requests(3)
{
}

EchoClient::~EchoClient()
{
	stop();
}

EchoClient::SendStatus EchoClient::send(std::string &&message)
{
	if (!isConnected())
		return DISCONNECTED;

	WriteRequest request;
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
	should_continue = false;
}

void EchoClient::connect(std::string_view server_address, uint16_t server_port)
{
	socket = std::make_unique<Socket>(server_address, server_port);

	connect_request.socket_fd = socket->raw();
	std::tie(connect_request.addr, connect_request.addrlen) = socket->getSockAddr();
	auto status = requests.add(connect_request);
	if (status == QUEUE_FULL)
		throw std::runtime_error("Error connecting: IO queue full");

	worker_thread = std::jthread([this]() { eventLoop(); });
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

void EchoClient::onCompletedConnect()
{
	read_request.fd = socket->raw();
	read_request.bytes_read = reception_buffer;
	AddRequestStatus status = requests.add(read_request);
	if (status == QUEUE_FULL)
		throw std::runtime_error("Error reading: IO queue full");

	{
		std::lock_guard<std::mutex> lock{ connection_mutex };
		is_connected = true;
		connection_cv.notify_all();
	}

	std::cout << "Client: Connected to server (socket " << socket->raw() << ")\n";
}

void EchoClient::onCompletedRead(const ReadRequest &completed_request)
{
	std::cout << "Client: Received \"" << reinterpret_cast<char *>(completed_request.bytes_read.data()) << "\""
		  << std::endl;
}

void EchoClient::onCompletedWrite(const WriteRequest &completed_request)
{
	std::cout << "Client: Sent \"" << reinterpret_cast<const char *>(completed_request.bytes_written.data()) << "\""
		  << std::endl;
	write_requests.clear();
}

void EchoClient::addPendingWriteRequests()
{
	{
		std::lock_guard lock(write_requests_mutex);
		if (write_requests.empty())
			return;

		for (const WriteRequest &request : write_requests)
			requests.add(request);
	}
}

void EchoClient::eventLoop()
{
	while (should_continue) {
		addPendingWriteRequests();
		SubmitStatus submit_status = requests.submit(std::chrono::milliseconds(100));

		if (requests.shouldContinueSubmitting(submit_status))
			continue;
		else if (submit_status < 0) {
			std::cerr << "Error submitting: " << strerror(-submit_status) << std::endl;
			continue;
		}

		requests.forEachCompletion([&](Completion cqe) {
			Operation op = getOperation(cqe);
			switch (op) {
			case Operation::CONNECT: {
				int connect_status = cqe->res;
				if (connect_status < 0) {
					std::cerr << "Client: Error connecting: " << strerror(-connect_status)
						  << std::endl;
					break;
				}
				onCompletedConnect();
			} break;
			case Operation::READ: {
				int read_status = cqe->res;
				if (read_status < 0) {
					std::cerr << "Client: Error reading: " << strerror(-read_status) << std::endl;
					break;
				}
				ReadRequest completed_request;
				memcpy(&completed_request, io_uring_cqe_get_data(cqe), sizeof(completed_request));
				onCompletedRead(completed_request);
			} break;
			case Operation::WRITE: {
				int write_status = cqe->res;
				if (write_status < 0) {
					std::cerr << "Client: Error writing: " << strerror(-write_status) << std::endl;
					break;
				}
				WriteRequest completed_request =
					*reinterpret_cast<WriteRequest *>(io_uring_cqe_get_data(cqe));
				onCompletedWrite(completed_request);
			} break;
			default:
				std::cerr << "Client: Invalid operation (" << static_cast<uint32_t>(op) << std::endl;
				break;
			}
		});
	}
}