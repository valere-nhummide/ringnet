#pragma once

#include "elio/net/baseSocket.hpp"

namespace elio::net::tcp
{

class ClientSocket : public BaseSocket<DatagramProtocol::TCP> {
    public:
	explicit ClientSocket(elio::EventLoop &loop_, std::string_view address_, uint16_t port)
		: BaseSocket<DatagramProtocol::TCP>(loop_, address_, port, false)
	{
	}

	int connect()
	{
		auto [addr, addrlen] = this->getSockAddr();
		return ::connect(this->fd, addr, addrlen);
	}
};
} // namespace elio::net::tcp