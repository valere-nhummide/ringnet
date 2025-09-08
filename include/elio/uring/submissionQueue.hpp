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
#include "elio/uring/pendingRequests.hpp"
#include "elio/uring/requestPool.hpp"
#include "elio/uring/requests.hpp"

namespace elio::uring
{

enum AddRequestStatus { OK = 0, QUEUE_FULL };
enum SubmitStatus : int { TIMEOUT = -ETIME, INTERRUPTED_SYSCALL = -EINTR, NOT_READY = -EAGAIN };

/// @brief Wrapper around the io_uring submission/completion queues.
/// When the he caller adds requests, it goes through the following cycles:
/// 1. Pushed to a waiting list of pending requests.
/// 2. On next loop iteration, prepared (io_uring_prep*)
/// 3. Submitted, by batch (io_uring_submit*)
/// 4. The corresponding completion entry is processed (io_uring_for_each_cqe)
class SubmissionQueue {
	RequestPool request_pool{};
	PendingRequests<AcceptRequest, ConnectRequest, ReadRequest, MultiShotReadRequest, WriteRequest>
		pending_requests{};

    public:
	explicit SubmissionQueue(size_t queue_size);
	~SubmissionQueue();

	template <class Request>
	void push(Request &&request)
	{
		Request *ptr = request_pool.allocate<Request>();
		*ptr = std::move(request);

		std::lock_guard<std::mutex> lock_guard(pending_requests_mutex);
		pending_requests.push(ptr);
	}

	void cancel(int fd);

	SubmitStatus submit(std::chrono::milliseconds timeout = {});

	static inline constexpr bool shouldContinueSubmitting(SubmitStatus status)
	{
		return status == TIMEOUT || status == INTERRUPTED_SYSCALL || status == NOT_READY;
	}

	template <class UnaryFunc>
	void forEachCompletion(UnaryFunc &&function);

	io_uring &getRing();

    private:
	/// @brief Prepare all pending requests, then clear the pending requests queue.
	/// @return The number of prepared requests.
	size_t preparePendingRequests();

	AddRequestStatus prepare(AcceptRequest *request);
	AddRequestStatus prepare(ConnectRequest *request);
	AddRequestStatus prepare(ReadRequest *request);
	AddRequestStatus prepare(MultiShotReadRequest *request);
	AddRequestStatus prepare(WriteRequest *request);
	std::mutex pending_requests_mutex{};

	io_uring ring{};

	/// @brief Get a new entry from the submission queue. If fails the first time, try to get room in the queue by
	/// submitting pending entries.
	/// @return A new submission entry if either first or second try succeeded. Otherwise, nullptr.
	/// @note The caller should check for nullptr.
	io_uring_sqe *getNewSubmissionQueueEntry();

	void release(io_uring_cqe *cqe);

	static void throwOnError(int liburing_error, std::string_view message);
};

// Template function definitions must remain in header
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
		release(cqe);
	}
	if (processed)
		io_uring_cq_advance(&ring, processed);
}

} // namespace elio::uring
