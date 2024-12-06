#pragma once

#include <string_view>

#include <cxxopts.hpp>

class CommandLineInterface {
    public:
	static constexpr const char *PROGRAM_NAME = "echo-tcp";
	static constexpr const char *PROGRAM_HELP = "Measure network throughput between local TCP client/server.";
	static constexpr std::string_view DEFAULT_ADDRESS = "127.0.0.1";
	static constexpr uint16_t DEFAULT_PORT = 6789;
	static constexpr uint64_t DEFAULT_BYTES_COUNT = 1'000'000'000;
	static constexpr uint64_t DEFAULT_CLIENTS_COUNT = 10;

	CommandLineInterface(int argc, char *argv[])
	{
		parse(argc, argv);
	}

	inline constexpr std::string_view address()
	{
		return DEFAULT_ADDRESS;
	}

	inline uint16_t port() const
	{
		return cli_arguments["port"].as<uint16_t>();
	}
	inline uint64_t bytes_count() const
	{
		return cli_arguments["bytes_count"].as<uint64_t>();
	}
	inline uint64_t clients_count() const
	{
		return cli_arguments["clients_count"].as<uint64_t>();
	}

    private:
	cxxopts::ParseResult cli_arguments{};

	void parse(int argc, char *argv[])
	{
		cxxopts::Options cli_options(PROGRAM_NAME, PROGRAM_HELP);

		cli_options.add_options()("p,port", "Port",
					  cxxopts::value<uint16_t>()->default_value(std::to_string(DEFAULT_PORT)));

		cli_options.add_options()(
			"b,bytes_count", "Bytes count",
			cxxopts::value<uint64_t>()->default_value(std::to_string(DEFAULT_BYTES_COUNT)));

		cli_options.add_options()(
			"c,clients_count", "Clients count",
			cxxopts::value<uint64_t>()->default_value(std::to_string(DEFAULT_CLIENTS_COUNT)));

		cli_arguments = cli_options.parse(argc, argv);
	}
};