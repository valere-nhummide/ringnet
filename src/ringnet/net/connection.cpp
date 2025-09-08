#include "ringnet/net/connection.hpp"

namespace ringnet::net
{

Connection::Connection(ringnet::EventLoop &loop_, FileDescriptor &&socket_)
	: loop(std::ref(loop_)), socket(std::move(socket_)), endpoint_{ .fd = socket.fd }
{
}

Connection::~Connection()
{
	if (socket)
		loop.get().cancel(socket.fd);
}

MessagedStatus Connection::asyncRead()
{
	ringnet::uring::MultiShotReadRequest request;
	request.fd = socket.fd;
	uring::AddRequestStatus status = loop.get().add(request, subscriber.get());
	if (status == ringnet::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

MessagedStatus Connection::asyncWrite(std::span<const std::byte> sent_bytes)
{
	ringnet::uring::WriteRequest request;
	request.fd = socket.fd;
	request.bytes_written = sent_bytes;
	uring::AddRequestStatus status = loop.get().add(request, subscriber.get());
	if (status == ringnet::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

const Endpoint &Connection::endpoint() const
{
	return endpoint_;
}

} // namespace ringnet::net