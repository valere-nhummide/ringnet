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
namespace ringnet::net
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
class FileDescriptor;

/// @brief Wrap the linux sockaddr_in or sockaddr_in6 in a variant and expose some utilities.
struct SocketAddress;

using SetOptionStatus = FileDescriptorStatus<strerror, true>;
SetOptionStatus set_option(FileDescriptor &socket, int option, bool enable = true);

using ResolveStatus = FileDescriptorStatus<gai_strerror>;
using ResolvedAddress = tl::expected<SocketAddress, ResolveStatus>;
ResolvedAddress resolve(std::string_view hostname, uint16_t port, DatagramProtocol datagram_protocol, bool passive);

using ConnectStatus = FileDescriptorStatus<strerror, true>;
ConnectStatus connect(FileDescriptor &socket, const SocketAddress &address);

using BindStatus = FileDescriptorStatus<strerror, true>;
BindStatus bind(FileDescriptor &socket, const SocketAddress &address);

using ListenStatus = FileDescriptorStatus<strerror, true>;
ListenStatus listen(FileDescriptor &socket, size_t max_pending_requests = std::numeric_limits<int>::max());

template <auto StrError, bool USE_ERRNO>
struct FileDescriptorStatus {
	static constexpr int SUCCESS = 0;
	explicit FileDescriptorStatus() : return_code(SUCCESS){};

	explicit FileDescriptorStatus(int return_code_) : return_code(USE_ERRNO ? errno : return_code_){};

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

class FileDescriptor {
    public:
	using Raw = int;
	Raw fd{ INVALID };

	FileDescriptor() = default;
	FileDescriptor(Raw fd);
	FileDescriptor(const FileDescriptor &) = delete;
	FileDescriptor &operator=(const FileDescriptor &) = delete;
	FileDescriptor(FileDescriptor &&);
	FileDescriptor &operator=(FileDescriptor &&);

	~FileDescriptor();

	inline explicit operator Raw() const;
	inline explicit operator bool() const;

    private:
	static constexpr Raw INVALID = -1;
};

inline FileDescriptor::operator Raw() const
{
	return fd;
}

inline FileDescriptor::operator bool() const
{
	return (fd > 0);
}

struct SocketAddress {
	std::variant<sockaddr_in, sockaddr_in6> underlying;

	std::pair<const sockaddr *, size_t> as_sockaddr() const;
	std::optional<IPVersion> ip_version() const;
};

} // namespace ringnet::net