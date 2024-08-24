# Launch a TCP echo server/client, sending predefined data. Measure throughput
# and latency.

set(TARGET corouring-echo-benchmark)
add_executable(${TARGET} src/main.cpp)

target_link_libraries(${TARGET} PRIVATE external::liburing)
