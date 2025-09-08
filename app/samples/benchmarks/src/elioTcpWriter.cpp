#include <string_view>

#include "elio_impl/elioTcpWriterClient.hpp"
#include "writerCLI.h"

int main(int argc, char *argv[])
{
	WriterCLI cli(argc, argv);

	const std::string_view address = cli.address();
	const uint16_t port = cli.port();
	const uint64_t chunk_size = cli.chunk_size();

	elio::EventLoop loop(1024);

	ElioTcpWriterClient client{ loop, chunk_size };
	client.connect(address, port);

	loop.run();
}