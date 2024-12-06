# Launch a TCP echo server/client, sending predefined data. Measure throughput
# and latency.

set(TARGET corouring-echo-benchmark)
add_executable(${TARGET} src/main.cpp)

target_include_directories(${TARGET}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${TARGET} PRIVATE elio external::cxxopts)
