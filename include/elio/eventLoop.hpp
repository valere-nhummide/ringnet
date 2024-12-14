#pragma once

#include <atomic>
#include <cassert>
#include <iostream>

#include "elio/eventHandler.hpp"
#include "elio/events.hpp"
#include "elio/status.hpp"
#include "elio/uring/bufferRing.hpp"
#include "elio/uring/requests.hpp"
#include "elio/uring/submissionQueue.hpp"

namespace elio
{

using Subscriber = EventHandler<events::ErrorEvent, events::AcceptEvent, events::ReadEvent, events::WriteEvent,
				events::ConnectEvent>;

class EventLoop {
    public:
	EventLoop(size_t request_queue_size);
	~EventLoop();

	void run();
	void stop();

	template <typename Resource, typename... Args>
	auto resource(Args &&...args);

	/// @brief Add a request to be prepared, then submitted. The associated subscriber will be notified once the
	/// request is completed.
	/// @tparam Request Type of the request
	/// @param request Content of the request
	/// @param subscriber Notified subscriber
	/// @return Cf. enumerate
	/// @warning The duration of both the request and the subscriber must outlive the completion.
	template <class Request>
	uring::AddRequestStatus add(std::shared_ptr<Request> &request, const std::shared_ptr<Subscriber> &subscriber);

	void cancel(int socket_fd);

    private:
	elio::uring::SubmissionQueue submission_queue;
	using Buffer = std::array<std::byte, 2048>;
	std::array<Buffer, 128> buffers{};
	elio::uring::BufferRing<Buffer> buffer_ring;
	std::atomic_bool should_continue{ true };

	using Completion = const io_uring_cqe *;

	template <class Request>
	inline static Request *getIssuingRequest(Completion cqe);

	inline static Subscriber *getAssociatedSubscriber(const uring::RequestHeader *header);

	template <class Stream = std::ostream>
	static void logIssuingRequest(Completion cqe, Stream &stream = std::cerr);
};

EventLoop::EventLoop(size_t request_queue_size)
	: submission_queue(request_queue_size), buffer_ring(submission_queue.getRing())
{
	MessagedStatus status = buffer_ring.setupBuffers(buffers);
	if (!status)
		std::cerr << status.what();
}

template <typename Resource, typename... Args>
auto EventLoop::resource(Args &&...args)
{
	return Resource(*this, std::forward<Args>(args)...);
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
			std::cerr << "Error submitting: " << strerror(-submit_status) << std::endl;
			continue;
		}

		submission_queue.forEachCompletion([this](Completion cqe) {
			if (!cqe->user_data) {
				std::cerr << "Error: Malformed completion queue entry" << std::endl;
				return;
			}

			const uring::RequestHeader *header = reinterpret_cast<const RequestHeader *>(cqe->user_data);
			if (!header->valid()) {
				std::cerr << "Error: Invalid request header" << std::endl;
				return;
			}

			Subscriber *subscriber = getAssociatedSubscriber(header);

			if (!subscriber) {
				std::cerr << "Error: No subscriber" << std::endl;
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
					std::cerr << "Error: Invalid buffer ID" << std::endl;
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
				std::cerr << "Error: Malformed completion queue entry" << std::endl;
				break;
			}
		});
	}
}

template <class Request>
inline Request *EventLoop::getIssuingRequest(Completion cqe)
{
	return reinterpret_cast<Request *>(cqe->user_data);
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

template <class Request>
uring::AddRequestStatus EventLoop::add(std::shared_ptr<Request> &request, const std::shared_ptr<Subscriber> &subscriber)
{
	if constexpr (std::is_same_v<Request, uring::MultiShotReadRequest>)
		request->buffer_group_id = buffer_ring.BUFFER_GROUP_ID;

	request->header.user_data = static_cast<void *>(subscriber.get());
	submission_queue.push(request);
	return uring::AddRequestStatus::OK;
}

template <class Stream>
void EventLoop::logIssuingRequest(Completion cqe, Stream &stream)
{
	using namespace elio::uring;
	uring::RequestHeader *header = reinterpret_cast<uring::RequestHeader *>(cqe->user_data);

	stream << "During handling of ";

	switch (header->op) {
	case Operation::ACCEPT: {
		stream << *getIssuingRequest<uring::AcceptRequest>(cqe);
	} break;
	case Operation::READ: {
		stream << *getIssuingRequest<uring::ReadRequest>(cqe);
	} break;
	case Operation::READ_MULTISHOT: {
		stream << *getIssuingRequest<uring::MultiShotReadRequest>(cqe);
	} break;
	case Operation::WRITE: {
		stream << *getIssuingRequest<uring::WriteRequest>(cqe);
	} break;
	case Operation::CONNECT: {
		stream << *getIssuingRequest<uring::ConnectRequest>(cqe);
	} break;
	default:
		stream << "malformed completion queue entry" << std::endl;
		break;
	}
	stream << std::endl;
}

} // namespace elio