#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string.h>
#include <string>

#include <liburing.h>

#include "elio/time/chronoUtils.hpp"
#include "elio/uring/refContainer.hpp"
#include "elio/uring/requests.hpp"

namespace elio::uring
{

enum AddRequestStatus { OK = 0, QUEUE_FULL };
enum SubmitStatus : int { TIMEOUT = -ETIME, INTERRUPTED_SYSCALL = -EINTR, NOT_READY = -EAGAIN };

/// @brief Wrapper around the io_uring submission/completion queues.
/// The caller adds requests:
/// 1. Push them to a waiting list of pending requests.
/// 2. Prepare the pending requests (io_uring_prep*)
/// 3. Submit them, by batch (io_uring_submit*)
/// 4. Handle their corresponding completion entries (io_uring_for_each_cqe)
class SubmissionQueue {
	RefContainer<AcceptRequest, ConnectRequest, ReadRequest, MultiShotReadRequest, WriteRequest> pending_requests{};

    public:
	explicit SubmissionQueue(size_t queue_size);
	~SubmissionQueue();

	template <class Request>
	void push(const Request &request)
	{
		std::lock_guard<std::mutex> lock_guard(pending_requests_mutex);
		pending_requests.push(request);
	}

	SubmitStatus submit(std::chrono::milliseconds timeout = {});

	static inline constexpr bool shouldContinueSubmitting(SubmitStatus status)
	{
		return status == TIMEOUT || status == INTERRUPTED_SYSCALL || status == NOT_READY;
	}

	template <class UnaryFunc>
	void forEachCompletion(UnaryFunc &&function);

	io_uring &getRing();

    private:
	void preparePendingRequests();
	AddRequestStatus prepare(const AcceptRequest &request);
	AddRequestStatus prepare(const ConnectRequest &request);
	AddRequestStatus prepare(const ReadRequest &request);
	AddRequestStatus prepare(const MultiShotReadRequest &request);
	AddRequestStatus prepare(const WriteRequest &request);
	std::mutex pending_requests_mutex{};

	io_uring ring{};

	/// @brief Get a new entry from the submission queue. If fails the first time, try to get room in the queue by
	/// submitting pending entries.
	/// @return A new submission entry if either first or second try succeeded. Otherwise, nullptr.
	/// @note The caller should check for nullptr.
	inline io_uring_sqe *getNewSubmissionQueueEntry();

	static inline void throwOnError(int liburing_error, std::string_view message)
	{
		if (liburing_error < 0)
			throw std::runtime_error(std::string(message) + ": " + std::string(strerror(-liburing_error)));
	}
};

SubmissionQueue::SubmissionQueue(size_t queue_size)
{
	throwOnError(io_uring_queue_init(queue_size, &ring, 0), "Error initializing io_uring");
}

SubmissionQueue::~SubmissionQueue()
{
	io_uring_queue_exit(&ring);
}

io_uring &SubmissionQueue::getRing()
{
	return ring;
}

void SubmissionQueue::preparePendingRequests()
{
	std::lock_guard<std::mutex> lock_guard(pending_requests_mutex);
	pending_requests.for_each([this](const auto &request) { prepare(request); });
	pending_requests.clear();
}

inline SubmitStatus SubmissionQueue::submit(std::chrono::milliseconds timeout)
{
	static constexpr unsigned WAITED_COMPLETIONS = 1;
	static constexpr sigset_t *BLOCKED_SIGNALS = nullptr;

	preparePendingRequests();

	if (timeout.count() <= 0)
		return SubmitStatus{ io_uring_submit_and_wait(&ring, WAITED_COMPLETIONS) };

	__kernel_timespec timeout_ = elio::time::chrono_utils::to_timespec(timeout);
	io_uring_cqe *completed_events = nullptr;
	int completions = io_uring_submit_and_wait_timeout(&ring, &completed_events, WAITED_COMPLETIONS, &timeout_,
							   BLOCKED_SIGNALS);

	if (completions > 0 && completed_events == nullptr) {
		int peek_status = io_uring_peek_cqe(&ring, &completed_events);
		if (peek_status < 0)
			return SubmitStatus{ peek_status };
	}

	return SubmitStatus{ completions };
}

template <class UnaryFunc>
void SubmissionQueue::forEachCompletion(UnaryFunc &&function)
{
	io_uring_cqe *cqe;
	uint head = 0;
	uint processed = 0;
	io_uring_for_each_cqe(&ring, head, cqe)
	{
		++processed;
		function(cqe);
	}
	if (processed)
		io_uring_cq_advance(&ring, processed);
}

AddRequestStatus SubmissionQueue::prepare(const AcceptRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_multishot_accept(sqe, request.listening_socket_fd, nullptr, nullptr, 0);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(const ConnectRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_connect(sqe, request.socket_fd, request.addr, request.addrlen);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(const WriteRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_write(sqe, request.fd, request.bytes_written.data(), request.bytes_written.size(), 0);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(const ReadRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_read(sqe, request.fd, request.reception_buffer.data(), request.reception_buffer.size(), 0);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(const MultiShotReadRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	sqe->flags |= IOSQE_BUFFER_SELECT;
	sqe->buf_group = request.buffer_group_id;

	io_uring_prep_read_multishot(sqe, request.fd, 0, 0, request.buffer_group_id);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

inline io_uring_sqe *SubmissionQueue::getNewSubmissionQueueEntry()
{
	io_uring_sqe *sqe = io_uring_get_sqe(&ring);

	if (!sqe) {
		io_uring_submit(&ring);
		sqe = io_uring_get_sqe(&ring);
	}
	return sqe;
}

} // namespace elio::uring