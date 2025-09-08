#pragma once

#include <cassert>
#include <iostream>
#include <map>
#include <string_view>

#include "elio/net/acceptor.hpp"
#include "elio/net/connection.hpp"
#include "elio/net/endpoint.hpp"

class ElioTcpReader {
    public:
	ElioTcpReader(elio::EventLoop &loop_, size_t target_bytes_count);

	void listen(std::string_view listening_address, uint16_t listening_port);
	void printResults();

    private:
	void registerCallbacks();
	void onRead(elio::events::ReadEvent &&event);

	elio::EventLoop &loop;
	elio::net::Acceptor<elio::net::TCP> acceptor;
	std::map<elio::net::Endpoint, elio::net::Connection> connections{};

	size_t target_bytes_count_;
	size_t received_bytes_count_{ 0 };
};