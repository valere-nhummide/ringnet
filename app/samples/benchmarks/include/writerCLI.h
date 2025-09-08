#pragma once

#include <iostream>
#include <string_view>

#include <cxxopts.hpp>

class WriterCLI {
    public:
	static constexpr const char *PROGRAM_NAME = "tcp-writer";
	static constexpr const char *PROGRAM_HELP = "TCP client sending data until stopped.";
	static constexpr std::string_view DEFAULT_ADDRESS = "127.0.0.1";
	static constexpr uint16_t DEFAULT_PORT = 6789;
	static constexpr uint64_t DEFAULT_CHUNK_SIZE = 1024;

	WriterCLI(int argc, char *argv[])
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
	inline uint64_t chunk_size() const
	{
		return cli_arguments["chunk_size"].as<uint64_t>();
	}

    private:
	cxxopts::ParseResult cli_arguments{};

	void parse(int argc, char *argv[])
	{
		cxxopts::Options cli_options(PROGRAM_NAME, PROGRAM_HELP);

		cli_options.add_options()("p,port", "Port",
					  cxxopts::value<uint16_t>()->default_value(std::to_string(DEFAULT_PORT)));

		cli_options.add_options()(
			"c,chunk_size", "Chunk size (bytes per write operation)",
			cxxopts::value<uint64_t>()->default_value(std::to_string(DEFAULT_CHUNK_SIZE)));

		cli_options.add_options()("h,help", "Print help and exit");

		cli_arguments = cli_options.parse(argc, argv);

		if (cli_arguments.count("help")) {
			std::cout << cli_options.help() << std::endl;
			exit(0);
		}
	}
};