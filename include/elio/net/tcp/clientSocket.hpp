#pragma once

#include "elio/net/baseSocket.hpp"

namespace elio::net::tcp
{

class ClientSocket : public BaseSocket<DatagramProtocol::TCP> {
    public:
	explicit ClientSocket(elio::EventLoop &loop_) : BaseSocket<DatagramProtocol::TCP>(loop_)
	{
	}

	explicit ClientSocket(elio::EventLoop &loop_, FileDescriptor fd_)
		: BaseSocket<DatagramProtocol::TCP>(loop_, fd_)
	{
	}
	ResolveStatus resolve(std::string_view address_, uint16_t port)
	{
		return _resolve(address_, port, true);
	}

	int connect()
	{
		auto [addr, addrlen] = this->getSockAddr();
		return ::connect(this->fd, addr, addrlen);
	}
};
} // namespace elio::net::tcp