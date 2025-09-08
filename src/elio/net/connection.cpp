#include "elio/net/connection.hpp"

namespace elio::net
{

Connection::Connection(elio::EventLoop &loop_, FileDescriptor &&socket_)
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
	elio::uring::MultiShotReadRequest request;
	request.fd = socket.fd;
	uring::AddRequestStatus status = loop.get().add(request, subscriber.get());
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

MessagedStatus Connection::asyncWrite(std::span<const std::byte> sent_bytes)
{
	elio::uring::WriteRequest request;
	request.fd = socket.fd;
	request.bytes_written = sent_bytes;
	uring::AddRequestStatus status = loop.get().add(request, subscriber.get());
	if (status == elio::uring::QUEUE_FULL)
		return MessagedStatus{ false, "Request queue is full" };

	return MessagedStatus{ true, "Success" };
}

const Endpoint &Connection::endpoint() const
{
	return endpoint_;
}

} // namespace elio::net