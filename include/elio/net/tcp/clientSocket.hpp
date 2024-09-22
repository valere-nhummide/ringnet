#pragma once

#include "elio/net/baseSocket.hpp"

namespace elio::net::tcp
{

class ClientSocket : public BaseSocket<DatagramProtocol::TCP> {
    public:
	explicit ClientSocket(std::string_view address_, uint16_t port)
		: BaseSocket<DatagramProtocol::TCP>(address_, port, false)
	{
	}

	int connect()
	{
		auto [addr, addrlen] = this->getSockAddr();
		return ::connect(this->fd, addr, addrlen);
	}
};
} // namespace elio::net::tcp