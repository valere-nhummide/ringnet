#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <netdb.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <type_traits>
#include <variant>

namespace elio::net
{
enum IPVersion : decltype(AF_UNSPEC) { UNKNOWN = AF_UNSPEC, IPv4 = AF_INET, IPv6 = AF_INET6 };

enum DatagramProtocol : std::underlying_type_t<decltype(SOCK_DGRAM)> { UDP = SOCK_DGRAM, TCP = SOCK_STREAM };

namespace detail
{
template <DatagramProtocol DP>
class BaseSocket {
    public:
	explicit BaseSocket(std::string_view address_, uint16_t port, bool should_listen)
	{
		std::tie(address, ip_version) = resolve(address_, port, should_listen);
		fd = socket(ip_version, DP, 0);
		if (fd < 0)
			throw std::runtime_error("Error creating socket: " + std::string(strerror(errno)));

		setOption(SO_REUSEADDR);
	}

	~BaseSocket()
	{
		if (fd > 0)
			close(fd);
		fd = -1;
	}

	inline int raw() const
	{
		return fd;
	}

	using address_t = std::variant<sockaddr_in, sockaddr_in6>;
	static std::pair<address_t, IPVersion> resolve(std::string_view address, uint16_t port, bool passive)
	{
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = DP;
		hints.ai_flags = passive ? AI_PASSIVE : 0;

		addrinfo *results = nullptr;
		auto addrinfo_deleter = [&](addrinfo *ptr) { ::freeaddrinfo(ptr); };
		std::unique_ptr<addrinfo, decltype(addrinfo_deleter)> results_raii_wrapper{ results, addrinfo_deleter };

		int status = ::getaddrinfo(address.data(), std::to_string(port).c_str(), &hints, &results);
		if (status != 0)
			throw std::runtime_error("Error resolving address " + std::string(address) + ":" +
						 std::to_string(port) + ": " + std::string(gai_strerror(status)));

		for (addrinfo *result = results; result; result = result->ai_next) {
			if (result->ai_family == AF_INET) {
				sockaddr_in addr_v4;
				memcpy(&addr_v4, result->ai_addr, sizeof(addr_v4));
				return std::make_pair(addr_v4, IPv4);
			} else if (result->ai_family == AF_INET6) {
				sockaddr_in6 addr_v6;
				memcpy(&addr_v6, result->ai_addr, sizeof(addr_v6));
				return std::make_pair(addr_v6, IPv6);
			}
		}

		return std::make_pair(address_t{}, UNKNOWN);
	}

	std::pair<sockaddr *, size_t> getSockAddr()
	{
		if (auto ptr_v4 = std::get_if<sockaddr_in>(&address); ptr_v4)
			return std::make_pair(reinterpret_cast<sockaddr *>(ptr_v4), sizeof(sockaddr_in));
		if (auto ptr_v6 = std::get_if<sockaddr_in6>(&address); ptr_v6)
			return std::make_pair(reinterpret_cast<sockaddr *>(ptr_v6), sizeof(sockaddr_in));

		return std::make_pair(nullptr, 0);
	}

    protected:
	int fd = -1;

    private:
	address_t address{};
	IPVersion ip_version = UNKNOWN;

	void setOption(int opt, bool enable = true)
	{
		int value = enable ? 1 : 0;
		::setsockopt(fd, SOL_SOCKET, opt, &value, sizeof(value));
	}
};
} // namespace detail

template <DatagramProtocol DP>
class ClientSocket : public detail::BaseSocket<DP> {
    public:
	explicit ClientSocket(std::string_view address_, uint16_t port) : detail::BaseSocket<DP>(address_, port, false)
	{
	}

	int connect()
	{
		auto [addr, addrlen] = this->getSockAddr();
		return ::connect(this->fd, addr, addrlen);
	}
};
template <DatagramProtocol DP>
class ServerSocket : public detail::BaseSocket<DP> {
    public:
	explicit ServerSocket(std::string_view address_, uint16_t port) : detail::BaseSocket<DP>(address_, port, true)
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
} // namespace elio::net