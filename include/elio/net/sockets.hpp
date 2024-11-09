#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <netdb.h>
#include <optional>
#include <span>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <type_traits>
#include <variant>

#include <tl/expected.hpp>

/// @brief Free methods to manipulate sockets. All those methods are blocking.
namespace elio::net
{
enum IPVersion : decltype(AF_UNSPEC) { UNKNOWN = AF_UNSPEC, IPv4 = AF_INET, IPv6 = AF_INET6 };
enum DatagramProtocol : std::underlying_type_t<decltype(SOCK_DGRAM)> { UDP = SOCK_DGRAM, TCP = SOCK_STREAM };

/// @brief Wrap Unix socket error codes and messages into a more user-friendly structure, exposing a "what" method
/// @tparam StrError The method to convert the code to a message (e.g. strerror)
/// @tparam USE_ERRNO If set, the error code is retrieved by calling errno. Otherwise the method's return code is used.
template <auto StrError = ::strerror, bool USE_ERRNO = false>
struct FileDescriptorStatus;

/// @brief Thin RAII wrapper around a file descriptor.
/// It stores the address once resolved.
class Socket;

/// @brief Wrap the linux sockaddr_in or sockaddr_in6 in a variant and expose some utilities.
struct SocketAddress;

using SetOptionStatus = FileDescriptorStatus<strerror, true>;
SetOptionStatus set_option(Socket &socket, int option, bool enable = true);

using ResolveStatus = FileDescriptorStatus<gai_strerror>;
using ResolvedAddress = tl::expected<SocketAddress, ResolveStatus>;
ResolvedAddress resolve(std::string_view hostname, uint16_t port, DatagramProtocol datagram_protocol, bool passive);

using ConnectStatus = FileDescriptorStatus<strerror, true>;
ConnectStatus connect(Socket &socket, const SocketAddress &address);

using BindStatus = FileDescriptorStatus<strerror, true>;
BindStatus bind(Socket &socket, const SocketAddress &address);

using ListenStatus = FileDescriptorStatus<strerror, true>;
ListenStatus listen(Socket &socket, size_t max_pending_requests = std::numeric_limits<int>::max());

template <auto StrError, bool USE_ERRNO>
struct FileDescriptorStatus {
	static constexpr int SUCCESS = 0;
	explicit FileDescriptorStatus() : return_code(SUCCESS) {};

	explicit FileDescriptorStatus(int return_code_) : return_code(USE_ERRNO ? errno : return_code_) {};

	inline operator bool() const
	{
		return return_code == SUCCESS;
	}
	inline const char *what() const
	{
		return StrError(return_code);
	}
	inline int code() const
	{
		return return_code;
	}

    private:
	int return_code;
};

class Socket {
    public:
	using FileDescriptor = int;
	FileDescriptor fd{ INVALID };

	Socket() = default;
	Socket(FileDescriptor fd);
	Socket(const Socket &) = delete;
	Socket &operator=(const Socket &) = delete;
	Socket(Socket &&);
	Socket &operator=(Socket &&);

	~Socket();

	inline explicit operator FileDescriptor() const;
	inline explicit operator bool() const;

    private:
	static constexpr FileDescriptor INVALID = -1;
};

Socket::Socket(FileDescriptor fd_) : fd(fd_)
{
}

Socket::Socket(Socket &&other) : fd(other.fd)
{
	other.fd = INVALID;
}

Socket &Socket::operator=(Socket &&other)
{
	using std::swap;
	swap(fd, other.fd);
	return *this;
}

Socket::~Socket()
{
	if (fd > 0)
		::close(fd);
	fd = INVALID;
}

inline Socket::operator FileDescriptor() const
{
	return fd;
}

inline Socket::operator bool() const
{
	return (fd > 0);
}

struct SocketAddress {
	std::variant<sockaddr_in, sockaddr_in6> underlying;

	inline std::pair<const sockaddr *, size_t> as_sockaddr() const;
	inline std::optional<IPVersion> ip_version() const;
};

std::pair<const sockaddr *, size_t> SocketAddress::as_sockaddr() const
{
	if (auto ptr_v4 = std::get_if<sockaddr_in>(&underlying); ptr_v4)
		return std::make_pair(reinterpret_cast<const sockaddr *>(ptr_v4), sizeof(sockaddr_in));
	if (auto ptr_v6 = std::get_if<sockaddr_in6>(&underlying); ptr_v6)
		return std::make_pair(reinterpret_cast<const sockaddr *>(ptr_v6), sizeof(sockaddr_in));

	return std::make_pair(nullptr, 0);
}

inline std::optional<IPVersion> SocketAddress::ip_version() const
{
	if (auto ptr_v4 = std::get_if<sockaddr_in>(&underlying); ptr_v4)
		return IPVersion::IPv4;
	if (auto ptr_v6 = std::get_if<sockaddr_in6>(&underlying); ptr_v6)
		return IPVersion::IPv6;
	return std::nullopt;
}

SetOptionStatus set_option(Socket &socket, int option, bool enable)
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

ConnectStatus connect(Socket &socket, const SocketAddress &address)
{
	auto [addr, addrlen] = address.as_sockaddr();
	return ConnectStatus{ ::connect(socket.fd, addr, addrlen) };
}

BindStatus bind(Socket &socket, const SocketAddress &address)
{
	auto [addr, addrlen] = address.as_sockaddr();
	return BindStatus{ ::bind(socket.fd, addr, addrlen) };
}

ListenStatus listen(Socket &socket, size_t max_pending_requests)
{
	return ListenStatus{ ::listen(socket.fd, max_pending_requests) };
}

} // namespace elio::net