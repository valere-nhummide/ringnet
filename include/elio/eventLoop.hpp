#pragma once

#include "elio/errorHandler.hpp"
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

	template <class Func>
	void onError(Func &&callback);

	template <typename Resource, typename... Args>
	Resource resource(Args &&...args);

	/// @brief Add a request to be prepared, then submitted. The associated subscriber will be notified once the
	/// request is completed.
	/// @tparam Request Type of the request
	/// @param request Content of the request
	/// @param subscriber Notified subscriber
	/// @return Cf. enumerate
	/// @warning The duration of both the request and the subscriber must outlive the completion.
	template <class Request>
	uring::AddRequestStatus add(Request &&request, Subscriber *subscriber);

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

	ErrorHandler error_handler{};

	template <class Stream = std::ostream>
	static void logIssuingRequest(Completion cqe, Stream &stream = std::cerr);
};

template <class Func>
void EventLoop::onError(Func &&callback)
{
	error_handler.onError(std::move(callback));
}

template <typename Resource, typename... Args>
Resource EventLoop::resource(Args &&...args)
{
	return Resource(*this, std::forward<Args>(args)...);
}

template <class Request>
inline Request *EventLoop::getIssuingRequest(Completion cqe)
{
	return reinterpret_cast<Request *>(cqe->user_data);
}

template <class Request>
uring::AddRequestStatus EventLoop::add(Request &&request, Subscriber *subscriber)
{
	if constexpr (std::is_same_v<std::decay_t<Request>, uring::MultiShotReadRequest>)
		request.buffer_group_id = buffer_ring.BUFFER_GROUP_ID;

	request.header.user_data = static_cast<void *>(subscriber);
	submission_queue.push(std::move(request));
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