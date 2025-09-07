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
