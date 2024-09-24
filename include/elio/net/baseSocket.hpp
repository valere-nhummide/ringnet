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

#include "elio/eventLoop.hpp"

namespace elio::net
{
using FileDescriptor = int;

enum IPVersion : decltype(AF_UNSPEC) { UNKNOWN = AF_UNSPEC, IPv4 = AF_INET, IPv6 = AF_INET6 };

enum DatagramProtocol : std::underlying_type_t<decltype(SOCK_DGRAM)> { UDP = SOCK_DGRAM, TCP = SOCK_STREAM };

using Address = std::variant<sockaddr_in, sockaddr_in6>;
using ResolveResult = std::pair<Address, IPVersion>;

template <DatagramProtocol DP>
class BaseSocket {
    public:
	explicit BaseSocket(elio::EventLoop &loop, std::string_view address_, uint16_t port, bool should_listen);

	~BaseSocket();

	enum class ReadStatus { OK, DISCONNECTED, QUEUE_FULL };
	ReadStatus read(std::span<std::byte> reception_buffer, elio::Subscriber &subscriber);

	inline FileDescriptor raw() const;

	static ResolveResult resolve(std::string_view address, uint16_t port, bool passive);

	std::pair<sockaddr *, size_t> getSockAddr();

    protected:
	FileDescriptor fd = -1;

    private:
	elio::EventLoop &loop;
	Address address{};
	IPVersion ip_version = UNKNOWN;

	/// @brief The read request is multi-shot, hence only one suffice.
	/// It should be renewed on errors. 
	elio::uring::ReadRequest read_request{};

	inline void setOption(int opt, bool enable = true);
};

template <DatagramProtocol DP>
BaseSocket<DP>::BaseSocket(elio::EventLoop &loop_, std::string_view address_, uint16_t port, bool should_listen)
	: loop(loop_)
{
	std::tie(address, ip_version) = resolve(address_, port, should_listen);
	fd = socket(ip_version, DP, 0);
	if (fd < 0)
		throw std::runtime_error("Error creating socket: " + std::string(strerror(errno)));

	setOption(SO_REUSEADDR);
}

template <DatagramProtocol DP>
BaseSocket<DP>::~BaseSocket()
{
	if (fd > 0)
		close(fd);
	fd = -1;
}

template <DatagramProtocol DP>
BaseSocket<DP>::ReadStatus BaseSocket<DP>::read(std::span<std::byte> reception_buffer, elio::Subscriber &subscriber)
{
	if (!fd)
		return ReadStatus::DISCONNECTED;

	read_request.fd = fd;
	read_request.bytes_read = reception_buffer;
	elio::uring::AddRequestStatus status = loop.add(read_request, subscriber);
	if (status == elio::uring::QUEUE_FULL)
		return ReadStatus::QUEUE_FULL;

	return ReadStatus::OK;
}

template <DatagramProtocol DP>
FileDescriptor BaseSocket<DP>::raw() const
{
	return fd;
}

template <DatagramProtocol DP>
ResolveResult BaseSocket<DP>::resolve(std::string_view address, uint16_t port, bool passive)
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

	return std::make_pair(Address{}, UNKNOWN);
}

template <DatagramProtocol DP>
std::pair<sockaddr *, size_t> BaseSocket<DP>::getSockAddr()
{
	if (auto ptr_v4 = std::get_if<sockaddr_in>(&address); ptr_v4)
		return std::make_pair(reinterpret_cast<sockaddr *>(ptr_v4), sizeof(sockaddr_in));
	if (auto ptr_v6 = std::get_if<sockaddr_in6>(&address); ptr_v6)
		return std::make_pair(reinterpret_cast<sockaddr *>(ptr_v6), sizeof(sockaddr_in));

	return std::make_pair(nullptr, 0);
}
template <DatagramProtocol DP>
void BaseSocket<DP>::setOption(int opt, bool enable)
{
	int value = enable ? 1 : 0;
	::setsockopt(fd, SOL_SOCKET, opt, &value, sizeof(value));
}
} // namespace elio::net