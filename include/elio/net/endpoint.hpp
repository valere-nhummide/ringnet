#pragma once

#include <string_view>

#include "elio/net/sockets.hpp"

namespace elio::net
{
class Endpoint {
    public:
	FileDescriptor::Raw fd;
};

bool operator<(Endpoint lhs, Endpoint rhs)
{
	return lhs.fd < rhs.fd;
}

} // namespace elio::net