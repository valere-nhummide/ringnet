# Launch a TCP echo server/client, sending predefined data. Measure throughput.
# Several implementations are available:
# - Using this library
# - Using the Asio library

set(TARGET-ELIO echo-benchmark-${PROJECT_NAME})
add_executable(${TARGET-ELIO} src/elioEchoBenchmark.cpp)

target_include_directories(${TARGET-ELIO}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ELIO} PRIVATE ${PROJECT_NAME} external::cxxopts)


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
set(TARGET-ELIO-TCP-WRITER tcp-writer-${PROJECT_NAME})
add_executable(${TARGET-ELIO-TCP-WRITER} src/elioTcpWriter.cpp src/elio_impl/elioTcpWriterClient.cpp)

target_include_directories(${TARGET-ELIO-TCP-WRITER}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ELIO-TCP-WRITER} PRIVATE ${PROJECT_NAME} external::cxxopts)


# Asio TCP reader: TCP server receiving data from a client until stopped.
set(TARGET-ASIO-TCP-READER tcp-reader-asio)
add_executable(${TARGET-ASIO-TCP-READER} src/asioTcpReader.cpp src/asio_impl/asioTcpReaderServer.cpp)

target_include_directories(${TARGET-ASIO-TCP-READER}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ASIO-TCP-READER} PRIVATE external::cxxopts external::asio)


# Elio TCP reader: TCP server receiving data from a client until stopped.
set(TARGET-ELIO-TCP-READER tcp-reader-${PROJECT_NAME})
add_executable(${TARGET-ELIO-TCP-READER} src/elioTcpReader.cpp src/elio_impl/elioTcpReaderServer.cpp)

target_include_directories(${TARGET-ELIO-TCP-READER}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ELIO-TCP-READER} PRIVATE ${PROJECT_NAME} external::cxxopts)
