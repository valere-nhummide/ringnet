#pragma once

#include "elio/net/baseSocket.hpp"

namespace elio::net::tcp
{

using BindStatus = FileDescriptorStatus<strerror, true>;
using ListenStatus = FileDescriptorStatus<strerror, true>;

class ServerSocket : public BaseSocket<DatagramProtocol::TCP> {
    public:
	explicit ServerSocket(elio::EventLoop &loop_) : BaseSocket<DatagramProtocol::TCP>(loop_)
	{
	}

	ResolveStatus resolve(std::string_view address_, uint16_t port)
	{
		return _resolve(address_, port, false);
	}

	int bind()
	{
		auto [addr, addrlen] = this->getSockAddr();
		return BindStatus{ ::bind(this->fd, addr, addrlen) };
	}

	int listen(size_t max_pending_requests = std::numeric_limits<int>::max())
	{
		return ListenStatus{ ::listen(this->fd, max_pending_requests) };
	}
};
} // namespace elio::net::tcp