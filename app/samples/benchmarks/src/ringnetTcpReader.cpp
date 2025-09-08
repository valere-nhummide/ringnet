#include <string_view>

#include "readerCLI.h"
#include "ringnet_impl/ringnetTcpReaderServer.hpp"

int main(int argc, char *argv[])
{
	ReaderCLI cli(argc, argv);

	ringnet::EventLoop loop(1024);

	RingnetTcpReader reader{ loop, cli.bytes_count() };
	reader.listen(cli.address(), cli.port());

	loop.run();
}