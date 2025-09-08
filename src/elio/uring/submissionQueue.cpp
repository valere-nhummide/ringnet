#include "elio/uring/submissionQueue.hpp"

namespace elio::uring
{

SubmissionQueue::SubmissionQueue(size_t queue_size)
{
	throwOnError(io_uring_queue_init(queue_size, &ring, 0), "Error initializing io_uring");
}

SubmissionQueue::~SubmissionQueue()
{
	io_uring_queue_exit(&ring);
}

void SubmissionQueue::cancel(int fd)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return;

	io_uring_prep_cancel_fd(sqe, fd, IORING_ASYNC_CANCEL_ALL | IORING_ASYNC_CANCEL_FD);
}

SubmitStatus SubmissionQueue::submit(std::chrono::milliseconds timeout)
{
	static constexpr unsigned WAITED_COMPLETIONS = 1;
	static constexpr sigset_t *BLOCKED_SIGNALS = nullptr;

	size_t prepared_requests_count = preparePendingRequests();

	if (prepared_requests_count == 0)
		return SubmitStatus{ NOT_READY };

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

io_uring &SubmissionQueue::getRing()
{
	return ring;
}

size_t SubmissionQueue::preparePendingRequests()
{
	std::lock_guard<std::mutex> lock_guard(pending_requests_mutex);
	size_t pending_requests_count = 0;
	pending_requests.for_each([this, &pending_requests_count](const auto &request) {
		++pending_requests_count;
		prepare(request);
	});
	pending_requests.clear();
	return pending_requests_count;
}

AddRequestStatus SubmissionQueue::prepare(AcceptRequest *request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_multishot_accept(sqe, request->listening_socket_fd, nullptr, nullptr, 0);
	io_uring_sqe_set_data(sqe, (void *)(request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(ConnectRequest *request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_connect(sqe, request->socket_fd, request->addr, request->addrlen);
	io_uring_sqe_set_data(sqe, (void *)(request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(WriteRequest *request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_write(sqe, request->fd, request->bytes_written.data(), request->bytes_written.size(), 0);
	io_uring_sqe_set_data(sqe, (void *)(request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(ReadRequest *request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	io_uring_prep_read(sqe, request->fd, request->reception_buffer.data(), request->reception_buffer.size(), 0);
	io_uring_sqe_set_data(sqe, (void *)(request));
	return OK;
}

AddRequestStatus SubmissionQueue::prepare(MultiShotReadRequest *request)
{
	io_uring_sqe *sqe = getNewSubmissionQueueEntry();

	if (!sqe)
		return QUEUE_FULL;

	sqe->flags |= IOSQE_BUFFER_SELECT;
	sqe->buf_group = request->buffer_group_id;

	io_uring_prep_read_multishot(sqe, request->fd, 0, 0, request->buffer_group_id);
	io_uring_sqe_set_data(sqe, (void *)(request));
	return OK;
}

io_uring_sqe *SubmissionQueue::getNewSubmissionQueueEntry()
{
	io_uring_sqe *sqe = io_uring_get_sqe(&ring);

	if (!sqe) {
		io_uring_submit(&ring);
		sqe = io_uring_get_sqe(&ring);
	}
	return sqe;
}

void SubmissionQueue::release(io_uring_cqe *cqe)
{
	const uring::RequestHeader *header = reinterpret_cast<const RequestHeader *>(cqe->user_data);
	if (!header->valid())
		return;

	switch (header->op) {
		// Do not release multi-shot requests
	case Operation::ACCEPT:
	case Operation::READ_MULTISHOT:
		return;
	case Operation::READ:
		request_pool.deallocate(reinterpret_cast<ReadRequest *>(cqe->user_data));
		return;
	case Operation::WRITE:
		request_pool.deallocate(reinterpret_cast<WriteRequest *>(cqe->user_data));
		return;
	case Operation::CONNECT:
		request_pool.deallocate(reinterpret_cast<ConnectRequest *>(cqe->user_data));
		return;
	}
}

void SubmissionQueue::throwOnError(int liburing_error, std::string_view message)
{
	if (liburing_error < 0)
		throw std::runtime_error(std::string(message) + ": " + std::string(strerror(-liburing_error)));
}

} // namespace elio::uring
