#pragma once

#include <string_view>

#include "ringnet/net/sockets.hpp"

namespace ringnet::net
{
class Endpoint {
    public:
	FileDescriptor::Raw fd;
};

inline bool operator<(Endpoint lhs, Endpoint rhs)
{
	return lhs.fd < rhs.fd;
}

} // namespace ringnet::net