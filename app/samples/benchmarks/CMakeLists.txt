# Launch a TCP echo server/client, sending predefined data. Measure throughput.
# Several implementations are available:
# - Using this library
# - Using the Asio library

set(TARGET-RINGNET echo-benchmark-${PROJECT_NAME})
add_executable(${TARGET-RINGNET} src/ringnetEchoBenchmark.cpp)

target_include_directories(${TARGET-RINGNET}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-RINGNET} PRIVATE ${PROJECT_NAME} external::cxxopts)


set(TARGET-ASIO echo-benchmark-asio)
add_executable(${TARGET-ASIO}
    src/asioEchoBenchmark.cpp
    src/asio_impl/asioEchoServer.cpp
    src/asio_impl/asioEchoClient.cpp
)

target_include_directories(${TARGET-ASIO}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ASIO} PRIVATE external::cxxopts external::asio)


# TCP writer: TCP client sending data to a server until stopped.
set(TARGET-RINGNET-TCP-WRITER tcp-writer-${PROJECT_NAME})
add_executable(${TARGET-RINGNET-TCP-WRITER} src/ringnetTcpWriter.cpp src/ringnet_impl/ringnetTcpWriterClient.cpp)

target_include_directories(${TARGET-RINGNET-TCP-WRITER}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-RINGNET-TCP-WRITER} PRIVATE ${PROJECT_NAME} external::cxxopts)


# Asio TCP reader: TCP server receiving data from a client until stopped.
set(TARGET-ASIO-TCP-READER tcp-reader-asio)
add_executable(${TARGET-ASIO-TCP-READER} src/asioTcpReader.cpp src/asio_impl/asioTcpReaderServer.cpp)

target_include_directories(${TARGET-ASIO-TCP-READER}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ASIO-TCP-READER} PRIVATE external::cxxopts external::asio)


# Ringnet TCP reader: TCP server receiving data from a client until stopped.
set(TARGET-RINGNET-TCP-READER tcp-reader-${PROJECT_NAME})
add_executable(${TARGET-RINGNET-TCP-READER} src/ringnetTcpReader.cpp src/ringnet_impl/ringnetTcpReaderServer.cpp)

target_include_directories(${TARGET-RINGNET-TCP-READER}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-RINGNET-TCP-READER} PRIVATE ${PROJECT_NAME} external::cxxopts)
