#pragma once

#include <atomic>
#include <cassert>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

#include "elio/net/connection.hpp"
#include "elio/net/connector.hpp"

class ElioTcpWriterClient {
    public:
	ElioTcpWriterClient(elio::EventLoop &loop, size_t chunk_size);

	void connect(std::string_view server_address, uint16_t server_port);

    private:
	void onConnected();

	elio::net::Connector<elio::net::TCP> connector;
	std::optional<elio::net::Connection> connection{};
	std::vector<std::byte> packet;
	static constexpr size_t BYTES_PRINT_INTERVAL = 100'000'000;
	int64_t remaining_bytes_before_print{ BYTES_PRINT_INTERVAL };
	size_t written_bytes{ 0 };
};
