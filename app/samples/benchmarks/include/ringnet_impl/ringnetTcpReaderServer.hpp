#pragma once

#include <cassert>
#include <iostream>
#include <map>
#include <string_view>

#include "ringnet/net/acceptor.hpp"
#include "ringnet/net/connection.hpp"
#include "ringnet/net/endpoint.hpp"

class RingnetTcpReader {
    public:
	RingnetTcpReader(ringnet::EventLoop &loop_, size_t target_bytes_count);

	void listen(std::string_view listening_address, uint16_t listening_port);
	void printResults();

    private:
	void registerCallbacks();
	void onRead(ringnet::events::ReadEvent &&event);

	ringnet::EventLoop &loop;
	ringnet::net::Acceptor<ringnet::net::TCP> acceptor;
	std::map<ringnet::net::Endpoint, ringnet::net::Connection> connections{};

	size_t target_bytes_count_;
	size_t received_bytes_count_{ 0 };
};