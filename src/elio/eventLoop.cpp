#include <cassert>

#include "elio/eventLoop.hpp"

namespace elio
{
EventLoop::EventLoop(size_t request_queue_size)
	: submission_queue(request_queue_size), buffer_ring(submission_queue.getRing())
{
	MessagedStatus status = buffer_ring.setupBuffers(buffers);
	if (!status)
		error_handler.handle(status.what());
}

void EventLoop::cancel(int socket_fd)
{
	submission_queue.cancel(socket_fd);
}

void EventLoop::run()
{
	using namespace elio::uring;

	while (should_continue) {
		SubmitStatus submit_status = submission_queue.submit(std::chrono::milliseconds(100));

		if (submission_queue.shouldContinueSubmitting(submit_status))
			continue;
		else if (submit_status < 0) {
			error_handler.handle(Error{ events::ErrorEvent{ .error_code = -submit_status } });
			continue;
		}

		submission_queue.forEachCompletion([this](Completion cqe) {
			if (!cqe->user_data) {
				error_handler.handle("Error: Malformed completion queue entry");
				return;
			}

			const uring::RequestHeader *header = reinterpret_cast<const RequestHeader *>(cqe->user_data);
			if (!header->valid()) {
				error_handler.handle("Error: Invalid request header");
				return;
			}

			Subscriber *subscriber = getAssociatedSubscriber(header);

			if (!subscriber) {
				error_handler.handle("Error: No subscriber");
				return;
			}

			if (cqe->res < 0) {
				/// @todo Provide this info in the error event instead, letting the subscriber log it.
				logIssuingRequest(cqe);
				subscriber->handle(events::ErrorEvent{ .error_code = -(cqe->res) });
				return;
			}

			switch (header->op) {
			case Operation::ACCEPT: {
				subscriber->handle(events::AcceptEvent{ .client_fd = cqe->res });
			} break;
			case Operation::READ: {
				auto request = getIssuingRequest<uring::ReadRequest>(cqe);

				// The result holds the number of bytes read.
				assert(static_cast<int>(request->reception_buffer.size()) >= cqe->res);
				assert(cqe->res >= 0);
				subscriber->handle(events::ReadEvent{
					.fd = request->fd,
					.bytes_read = request->reception_buffer.subspan(0, cqe->res) });

			} break;
			case Operation::READ_MULTISHOT: {
				auto request = getIssuingRequest<uring::MultiShotReadRequest>(cqe);
				// The result holds the number of bytes read.
				assert(cqe->res >= 0);

				auto buffer_view = buffer_ring.get(cqe);
				if (!buffer_view.has_value()) {
					error_handler.handle("Error: Invalid buffer ID");
					break;
				}
				subscriber->handle(events::ReadEvent{
					.fd = request->fd, .bytes_read = buffer_view->subspan(0, cqe->res) });
				buffer_ring.release(cqe);
			} break;
			case Operation::WRITE: {
				auto request = getIssuingRequest<uring::WriteRequest>(cqe);
				assert(static_cast<int>(request->bytes_written.size()) >= cqe->res);
				subscriber->handle(
					events::WriteEvent{ .fd = cqe->res, .bytes_written = request->bytes_written });
			} break;
			case Operation::CONNECT: {
				subscriber->handle(events::ConnectEvent{});
			} break;
			default:
				error_handler.handle("Error: Malformed completion queue entry");
				break;
			}
		});
	}
}

inline Subscriber *EventLoop::getAssociatedSubscriber(const uring::RequestHeader *header)
{
	return static_cast<Subscriber *>(header->user_data);
}

EventLoop::~EventLoop()
{
	stop();
}

void EventLoop::stop()
{
	should_continue = false;
}

} // namespace elio