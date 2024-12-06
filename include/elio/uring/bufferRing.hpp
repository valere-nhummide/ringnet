#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <liburing.h>

#include "elio/status.hpp"

namespace elio::uring
{
namespace raii
{

}
template <class Buffer>
class BufferRing {
	static_assert(sizeof(typename Buffer::value_type) == 1, "Buffer element type must be of byte size");
	using BufferView = std::span<typename Buffer::value_type>;
	static constexpr uint FLAGS = 0;

    public:
	static constexpr int BUFFER_GROUP_ID = 1;
	BufferRing(io_uring &io_ring_) : io_ring(std::ref(io_ring_))
	{
	}

	MessagedStatus setupBuffers(std::span<Buffer> buffers_)
	{
		if (!isPowerOfTwo(buffers_.size()))
			return MessagedStatus{ false, "The number of entries must be a power of two" };

		if (buffer_ring)
			io_uring_free_buf_ring(&(io_ring.get()), buffer_ring, buffers.size(), BUFFER_GROUP_ID);

		buffers = buffers_;

		int status{};
		buffer_ring =
			io_uring_setup_buf_ring(&(io_ring.get()), buffers.size(), BUFFER_GROUP_ID, FLAGS, &status);

		if (!buffer_ring)
			return MessagedStatus{ false, strerror(-status) };

		for (size_t buffer_id = 0; buffer_id < buffers.size(); buffer_id++) {
			Buffer &buffer = buffers[buffer_id];
			io_uring_buf_ring_add(buffer_ring, buffer.data(), buffer.size(), buffer_id,
					      io_uring_buf_ring_mask(buffers.size()), buffer_id);
		}
		io_uring_buf_ring_advance(buffer_ring, buffers.size());

		return MessagedStatus{ true, "Buffers added" };
	}

	BufferRing(const BufferRing &) = delete;
	BufferRing &operator=(const BufferRing &) = delete;

	~BufferRing()
	{
		if (buffer_ring)
			io_uring_free_buf_ring(&(io_ring.get()), buffer_ring, buffers.size(), BUFFER_GROUP_ID);
	}

	std::optional<BufferView> get(const io_uring_cqe *cqe)
	{
		if (!(cqe->flags | IORING_CQE_F_BUFFER))
			return std::nullopt;

		const int buffer_id = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
		if (buffer_id < 0 || buffer_id > (static_cast<int>(buffers.size()) - 1))
			return std::nullopt;

		return buffers[buffer_id];
	}

	void release(const io_uring_cqe *cqe)
	{
		if (cqe->flags | IORING_CQE_F_BUFFER) {
			const size_t buffer_id = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
			Buffer &buffer = buffers[buffer_id];
			io_uring_buf_ring_add(buffer_ring, buffer.data(), buffer.size(), buffer_id,
					      io_uring_buf_ring_mask(buffers.size()), 0);
			io_uring_buf_ring_advance(buffer_ring, 1);
		}
	}

    private:
	std::reference_wrapper<io_uring> io_ring;
	io_uring_buf_ring *buffer_ring = nullptr;
	std::span<Buffer> buffers{};

	inline bool isPowerOfTwo(size_t n)
	{
		return ((n & (n - 1)) == 0);
	}
};
} // namespace elio::uring