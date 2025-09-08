#include "ringnet/net/sockets.hpp"

namespace ringnet::net
{

FileDescriptor::FileDescriptor(Raw fd_) : fd(fd_)
{
}

FileDescriptor::FileDescriptor(FileDescriptor &&other) : fd(other.fd)
{
	other.fd = INVALID;
}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&other)
{
	using std::swap;
	swap(fd, other.fd);
	return *this;
}

FileDescriptor::~FileDescriptor()
{
	if (fd > 0)
		::close(fd);
	fd = INVALID;
}

std::pair<const sockaddr *, size_t> SocketAddress::as_sockaddr() const
{
	if (auto ptr_v4 = std::get_if<sockaddr_in>(&underlying); ptr_v4)
		return std::make_pair(reinterpret_cast<const sockaddr *>(ptr_v4), sizeof(sockaddr_in));
	if (auto ptr_v6 = std::get_if<sockaddr_in6>(&underlying); ptr_v6)
		return std::make_pair(reinterpret_cast<const sockaddr *>(ptr_v6), sizeof(sockaddr_in));

	return std::make_pair(nullptr, 0);
}

std::optional<IPVersion> SocketAddress::ip_version() const
{
	if (auto ptr_v4 = std::get_if<sockaddr_in>(&underlying); ptr_v4)
		return IPVersion::IPv4;
	if (auto ptr_v6 = std::get_if<sockaddr_in6>(&underlying); ptr_v6)
		return IPVersion::IPv6;
	return std::nullopt;
}

SetOptionStatus set_option(FileDescriptor &socket, int option, bool enable)
{
	int value = enable ? 1 : 0;
	return SetOptionStatus{ ::setsockopt(socket.fd, SOL_SOCKET, option, &value, sizeof(value)) };
}

ResolvedAddress resolve(std::string_view hostname, uint16_t port, DatagramProtocol datagram_protocol, bool passive)
{
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = datagram_protocol;
	hints.ai_flags = passive ? AI_PASSIVE : 0;

	addrinfo *results = nullptr;
	auto addrinfo_deleter = [&](addrinfo *ptr) { ::freeaddrinfo(ptr); };
	std::unique_ptr<addrinfo, decltype(addrinfo_deleter)> results_raii_wrapper{ results, addrinfo_deleter };

	int status = ::getaddrinfo(hostname.data(), std::to_string(port).c_str(), &hints, &results);
	if (status != 0)
		return tl::unexpected<ResolveStatus>(status);

	SocketAddress address{};
	for (addrinfo *result = results; result; result = result->ai_next) {
		if (result->ai_family == AF_INET) {
			sockaddr_in addr_v4;
			memcpy(&addr_v4, result->ai_addr, sizeof(addr_v4));
			address.underlying = addr_v4;
			break;
		} else {
			assert(result->ai_family == AF_INET6);
			sockaddr_in6 addr_v6;
			memcpy(&addr_v6, result->ai_addr, sizeof(addr_v6));
			address.underlying = addr_v6;
			break;
		}
	}

	return address;
}

namespace detail
{

} // namespace detail

ConnectStatus connect(FileDescriptor &socket, const SocketAddress &address)
{
	auto [addr, addrlen] = address.as_sockaddr();
	return ConnectStatus{ ::connect(socket.fd, addr, addrlen) };
}

BindStatus bind(FileDescriptor &socket, const SocketAddress &address)
{
	auto [addr, addrlen] = address.as_sockaddr();
	return BindStatus{ ::bind(socket.fd, addr, addrlen) };
}

ListenStatus listen(FileDescriptor &socket, size_t max_pending_requests)
{
	return ListenStatus{ ::listen(socket.fd, max_pending_requests) };
}

} // namespace ringnet::net