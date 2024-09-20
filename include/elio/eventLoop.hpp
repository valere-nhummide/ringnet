#pragma once

#include <atomic>

#include "subscriber.hpp"
#include "uring/requestQueue.hpp"

namespace elio
{
class EventLoop {
    public:
	EventLoop(size_t request_queue_size);
	~EventLoop();

	void run();
	void stop();

	/// @todo Make this private once the API allows to add requests
	elio::uring::RequestQueue requests;

    private:
	std::atomic_bool should_continue{ true };

	using Event = const io_uring_cqe *;
	inline static uring::RequestHeader getIssuingRequest(Event event);
	inline static Subscriber *getAssociatedSubscriber(uring::RequestHeader header);
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
		requests.forEachCompletion([this](Event event) {
			uring::RequestHeader request = getIssuingRequest(event);
			switch (request.op) {
			case Operation::ACCEPT: {
				if (event->res < 0) {
					std::cerr << "Error accepting: " << strerror(-(event->res)) << std::endl;
					/// @todo : Check if the request should be renewed
					return;
				}
				Subscriber *subscriber = getAssociatedSubscriber(request);
				AcceptRequest::Data *request_data =
					&(reinterpret_cast<AcceptRequest *>(io_uring_cqe_get_data(event))->data);

				if (subscriber)
					subscriber->handle(std::move(*request_data));
			} break;
			case Operation::READ: {
				if (event->res < 0) {
					std::cerr << "Error reading: " << strerror(-(event->res)) << std::endl;
					/// @todo : Check if the request should be renewed
					return;
				}
				Subscriber *subscriber = getAssociatedSubscriber(request);
				ReadRequest::Data *request_data =
					&(reinterpret_cast<ReadRequest *>(io_uring_cqe_get_data(event))->data);

				if (subscriber)
					subscriber->handle(std::move(*request_data));
			} break;
			case Operation::WRITE: {
				if (event->res < 0) {
					std::cerr << "Error writing: " << strerror(-(event->res)) << std::endl;
					/// @todo : Check if the request should be renewed
					return;
				}
				Subscriber *subscriber = getAssociatedSubscriber(request);
				WriteRequest::Data *request_data =
					&(reinterpret_cast<WriteRequest *>(io_uring_cqe_get_data(event))->data);

				if (subscriber)
					subscriber->handle(std::move(*request_data));
			} break;
			case Operation::CONNECT: {
				if (event->res < 0) {
					std::cerr << "Error connecting: " << strerror(-(event->res)) << std::endl;
					/// @todo : Check if the request should be renewed
					return;
				}
				Subscriber *subscriber = getAssociatedSubscriber(request);
				ConnectRequest::Data *request_data =
					&(reinterpret_cast<ConnectRequest *>(io_uring_cqe_get_data(event))->data);

				if (subscriber)
					subscriber->handle(std::move(*request_data));
			} break;
			default:
				std::cerr << "Invalid operation (" << static_cast<uint32_t>(request.op) << std::endl;
				break;
			}
		});
	}
}

inline uring::RequestHeader EventLoop::getIssuingRequest(Event event)
{
	uring::RequestHeader request;
	std::memcpy(&request, io_uring_cqe_get_data(event), sizeof(request));
	return request;
}

inline Subscriber *EventLoop::getAssociatedSubscriber(uring::RequestHeader header)
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
} // namespace elio