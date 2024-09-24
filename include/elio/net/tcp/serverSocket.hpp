#pragma once

#include "elio/net/baseSocket.hpp"

namespace elio::net::tcp
{

class ServerSocket : public BaseSocket<DatagramProtocol::TCP> {
    public:
	explicit ServerSocket(elio::EventLoop &loop_, std::string_view address_, uint16_t port)
		: BaseSocket<DatagramProtocol::TCP>(loop_, address_, port, true)
	{
	}

	int bind()
	{
		auto [addr, addrlen] = this->getSockAddr();
		return ::bind(this->fd, addr, addrlen);
	}

	int listen(size_t max_pending_requests = std::numeric_limits<int>::max())
	{
		return ::listen(this->fd, max_pending_requests);
	}
};
} // namespace elio::net::tcp