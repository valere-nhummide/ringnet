#include <string_view>

#include "asio_impl/asioTcpReaderServer.hpp"
#include "readerCLI.h"

int main(int argc, char *argv[])
{
	ReaderCLI cli(argc, argv);

	asio::io_context io_context;

	AsioTcpReader reader{ io_context, cli.bytes_count() };
	reader.listen(cli.address(), cli.port());

	io_context.run();
}