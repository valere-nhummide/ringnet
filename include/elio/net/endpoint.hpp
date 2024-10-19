#pragma once

#include <string_view>

#include "elio/net/sockets.hpp"

namespace elio::net
{
class Endpoint {
    public:
	Socket::FileDescriptor fd;
};

bool operator<(Endpoint lhs, Endpoint rhs)
{
	return lhs.fd < rhs.fd;
}

} // namespace elio::net