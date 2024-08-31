#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string.h>
#include <string>

#include <liburing.h>

#include "elio/time/chronoUtils.hpp"
#include "elio/uring/request.hpp"

namespace elio::uring
{

enum AddRequestStatus { OK = 0, QUEUE_FULL };
enum SubmitStatus : int { TIMEOUT = -ETIME, INTERRUPTED_SYSCALL = -EINTR, NOT_READY = -EAGAIN };

using Completion = io_uring_cqe *;

class RequestQueue {
    public:
	explicit RequestQueue(size_t queue_size);
	~RequestQueue();

	SubmitStatus submit(std::chrono::milliseconds timeout = {});

	static inline constexpr bool shouldContinueSubmitting(SubmitStatus status)
	{
		return status == TIMEOUT || status == INTERRUPTED_SYSCALL || status == NOT_READY;
	}

	template <class UnaryFunc>
	void forEachCompletion(UnaryFunc &&function);

	AddRequestStatus add(const AcceptRequest &request);
	AddRequestStatus add(const ConnectRequest &request);
	AddRequestStatus add(const ReadRequest &request);
	AddRequestStatus add(const WriteRequest &request);

    private:
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

RequestQueue::RequestQueue(size_t queue_size)
{
	throwOnError(io_uring_queue_init(queue_size, &ring, 0), "Error initializing io_uring");
}

RequestQueue::~RequestQueue()
{
	io_uring_queue_exit(&ring);
}

inline SubmitStatus RequestQueue::submit(std::chrono::milliseconds timeout)
{
	static constexpr unsigned WAITED_COMPLETIONS = 1;
	static constexpr sigset_t *BLOCKED_SIGNALS = nullptr;

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
void RequestQueue::forEachCompletion(UnaryFunc &&function)
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

AddRequestStatus RequestQueue::add(const AcceptRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_multishot_accept(sqe, request.listening_socket_fd, nullptr, nullptr, 0);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus RequestQueue::add(const ConnectRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_connect(sqe, request.socket_fd, request.addr, request.addrlen);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus RequestQueue::add(const WriteRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_write(sqe, request.fd, request.bytes_written.data(), request.bytes_written.size(), 0);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

AddRequestStatus RequestQueue::add(const ReadRequest &request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_read(sqe, request.fd, request.bytes_read.data(), request.bytes_read.size(), 0);
	io_uring_sqe_set_data(sqe, (void *)(&request));
	return OK;
}

inline io_uring_sqe *RequestQueue::getNewSubmissionQueueEntry()
{
	io_uring_sqe *sqe = io_uring_get_sqe(&ring);

	if (!sqe) {
		io_uring_submit(&ring);
		sqe = io_uring_get_sqe(&ring);
	}
	return sqe;
}

}