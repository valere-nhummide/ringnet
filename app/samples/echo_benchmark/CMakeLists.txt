# Launch a TCP echo server/client, sending predefined data. Measure throughput.
# Several implementations are available:
# - Using this library
# - Using the Asio library

set(TARGET ${PROJECT_NAME}-echo-benchmark)
add_executable(${TARGET} src/elio_impl.cpp)

target_include_directories(${TARGET}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET} PRIVATE ${PROJECT_NAME} external::cxxopts)


set(TARGET-ASIO ${PROJECT_NAME}-echo-benchmark-asio)
add_executable(${TARGET-ASIO} 
    src/asio_impl.cpp
    src/asio_impl/server.cpp
    src/asio_impl/client.cpp
)

target_include_directories(${TARGET-ASIO}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET-ASIO} PRIVATE external::cxxopts external::asio)
