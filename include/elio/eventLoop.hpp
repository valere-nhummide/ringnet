#pragma once

#include <atomic>
#include <cassert>

#include "elio/events.hpp"
#include "elio/subscriber.hpp"
#include "elio/uring/requestQueue.hpp"

namespace elio
{
class EventLoop {
    public:
	EventLoop(size_t request_queue_size);
	~EventLoop();

	void run();
	void stop();

	/// @brief Add a request to the queue. The associated subscriber will be notified once the request is completed.
	/// @tparam Request Type of the request
	/// @param request Content of the request
	/// @param subscriber Notified subscriber
	/// @return Cf. enumerate
	/// @warning The duration of both the request and the subscriber must outlive the completion.
	template <class Request>
	uring::AddRequestStatus add(Request &request, Subscriber &subscriber);

    private:
	elio::uring::RequestQueue requests;
	std::atomic_bool should_continue{ true };

	using Completion = const io_uring_cqe *;

	inline static uring::RequestHeader getIssuingRequestHeader(Completion cqe);

	template <class Request>
	inline static const Request *getIssuingRequest(Completion cqe);

	inline static Subscriber *getAssociatedSubscriber(const uring::RequestHeader &header);
};

EventLoop::EventLoop(size_t request_queue_size) : requests(request_queue_size)
{
}

void EventLoop::run()
{
	using namespace elio::uring;

	while (should_continue) {
		SubmitStatus submit_status = requests.submit(std::chrono::milliseconds(100));

		if (requests.shouldContinueSubmitting(submit_status))
			continue;
		else if (submit_status < 0) {
			std::cerr << "Error submitting: " << strerror(-submit_status) << std::endl;
			continue;
		}

		requests.forEachCompletion([](Completion cqe) {
			if (!cqe->user_data) {
				std::cerr << "Error: Malformed completion queue entry" << std::endl;
				return;
			}
			uring::RequestHeader *header = reinterpret_cast<uring::RequestHeader *>(cqe->user_data);
			Subscriber *subscriber = getAssociatedSubscriber(*header);

			if (!subscriber) {
				std::cerr << "Error: No subscriber" << std::endl;
				return;
			}

			if (cqe->res < 0) {
				/// @todo Move to user handler
				std::cerr << "Error: " << strerror(-(cqe->res)) << std::endl;
				subscriber->handle(events::ErrorEvent{ .error_code = -(cqe->res) });
				return;
			}

			switch (header->op) {
			case Operation::ACCEPT: {
				subscriber->handle(events::AcceptEvent{ .client_fd = cqe->res });
			} break;
			case Operation::READ: {
				auto request = getIssuingRequest<uring::ReadRequest>(cqe);
				assert(request->data.fd == cqe->res);
				subscriber->handle(
					events::ReadEvent{ .fd = cqe->res, .bytes_read = request->data.bytes_read });
			} break;
			case Operation::WRITE: {
				auto request = getIssuingRequest<uring::WriteRequest>(cqe);
				assert(request->data.fd == cqe->res);
				subscriber->handle(
					events::WriteEvent{ .fd = cqe->res, .bytes_written = { /* to fill */ } });
			} break;
			case Operation::CONNECT: {
				subscriber->handle(events::ConnectEvent{ .socket_fd = cqe->res });
			} break;
			default:
				std::cerr << "Error: Malformed completion queue entry" << std::endl;
				break;
			}
		});
	}
}

inline uring::RequestHeader EventLoop::getIssuingRequestHeader(Completion cqe)
{
	uring::RequestHeader request;
	std::memcpy(&request, io_uring_cqe_get_data(cqe), sizeof(request));
	return request;
}

template <class Request>
inline const Request *EventLoop::getIssuingRequest(Completion cqe)
{
	return reinterpret_cast<Request *>(cqe->user_data);
}

inline Subscriber *EventLoop::getAssociatedSubscriber(const uring::RequestHeader &header)
{
	return static_cast<Subscriber *>(header.user_data);
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
uring::AddRequestStatus EventLoop::add(Request &request, Subscriber &subscriber)
{
	request.header.user_data = static_cast<void *>(&subscriber);
	return requests.add(request);
}

} // namespace elio