#include <string_view>

#include "elio_impl/elioTcpReaderServer.hpp"
#include "readerCLI.h"

int main(int argc, char *argv[])
{
	ReaderCLI cli(argc, argv);

	elio::EventLoop loop(1024);

	ElioTcpReader reader{ loop, cli.bytes_count() };
	reader.listen(cli.address(), cli.port());

	loop.run();
}