# RingNet

A high-performance C++ networking library built on top of Linux io_uring for asynchronous I/O operations.

## Overview

RingNet is a C++20 networking library that leverages Linux's io_uring interface to provide efficient asynchronous I/O operations. The library implements advanced io_uring features such as batching, multi-shot operations, and provided buffers to achieve ultra-low latency and zero-copy networking.

It offers an event-driven architecture similar to existing asynchronous I/O libraries, but specifically optimized for io_uring's capabilities. This includes advanced buffer management features that may limit cross-platform compatibility but provide significant performance benefits on Linux systems.

## Features

### 🚧 Currently in Development

- **Callback-based TCP networking** - Event-driven TCP client and server implementations
- **Performance benchmarking** - Comprehensive benchmarks against industry-standard libraries
- **Robust error handling** - Comprehensive error management and reporting system

### 🎯 Planned Features

- **Portable API design** - Clean API that facilitates migration from/to other networking libraries
- **Performance optimizations** - Advanced optimizations leveraging io_uring's unique capabilities
- **C++20 coroutine support** - Native coroutine integration for simplified async programming
- **UDP networking** - High-performance UDP client and server implementations
- **File I/O support** - Optional file operations (secondary priority)
- **Latency monitoring** - Built-in tools for measuring and analyzing network latency

## Requirements

- Linux kernel with io_uring support (6.0+)
- C++20 compatible compiler
- CMake 3.20+

## Build

The project uses CMake for build configuration. See the root `CMakeLists.txt` for available build options.

```bash
cmake -S . -B build -DRINGNET_BUILD_SAMPLES=ON -DRINGNET_BUILD_TESTS=ON
cmake --build build
```

## Usage Example

See the sample applications in the `app/samples` folder for usage examples and best practices.

## Performance

The primary goal of this library is to fully leverage io_uring's advanced features, as described by Jens Axboe in [io_uring and networking in 2023](https://github.com/axboe/liburing/wiki/io_uring-and-networking-in-2023).  

The library includes comprehensive benchmarking tools that compare performance against ASIO, which also supports io_uring. As of December 2024, ASIO does not implement all advanced io_uring features, making it an ideal reference point for performance comparisons and validation.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Development Status

This project is currently under active development.

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## Author

**Valère Nhummide** - *Initial work* - [valere-nhummide](https://github.com/valere-nhummide)
